// Tarea 1 (maestro): comunicacion Modbus.
//
// Genera las solicitudes, las transmite, espera y procesa las respuestas, y
// detecta errores y timeouts. Trabaja de dos formas:
//   - a pedido: cada pedido_t que llega por cola_pedidos (desde la consola);
//   - sondeo ciclico (si esta activo): lee 40001..40004 de cada esclavo de
//     MB_ESCLAVOS_SONDEO, uno por vez y alternando.
// Un error nunca detiene la tarea: se informa y se sigue con el proximo pedido.

#include "tarea_modbus.h"

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "app_config.h"
#include "app_shared.h"
#include "crc16_modbus.h"
#include "modbus_maestro.h"

// Se usa printf para los datos (tablas prolijas) y ESP_LOGx para los errores.

static const char *TAG = "TAREA_MODBUS";

static const uint8_t esclavos_sondeo[] = MB_ESCLAVOS_SONDEO;
#define CANT_ESCLAVOS_SONDEO    (sizeof(esclavos_sondeo) / sizeof(esclavos_sondeo[0]))

static const char *nombre_registro[MB_CANT_REG_HOLDING] = {
    "Analogico 1 (ADC crudo)",
    "Analogico 2 (mV)",
    "Contador",
    "Estado (LED1)",
    "Setpoint (mV)",
    "Tiempo (ms)",
    "Modo (LED2)",
};

// Estado de enlace de cada direccion: -1 desconocido, 0 sin respuesta, 1 en linea.
// Sirve para avisar solo cuando un esclavo se cae o vuelve, no en cada consulta.
static int8_t en_linea[MB_DIR_MAX + 1];

static bool mostrar_tramas = false;

// ---------------------------------------------------------------------------
// Tramas (comando "tramas on"): se arman aca los mismos bytes que genera el
// stack, con su CRC, para compararlos con la captura de la PC.
// ---------------------------------------------------------------------------

static void imprimir_bytes(const char *titulo, const uint8_t *b, size_t n)
{
    printf("   %s:", titulo);
    for (size_t i = 0; i < n; i++) {
        printf(" %02X", b[i]);
    }
    printf("\n");
}

static void imprimir_trama_pedido(const pedido_t *p, uint8_t funcion)
{
    uint8_t t[9 + 2 * MB_MAX_REG_POR_PEDIDO + 2];
    size_t n = 0;

    t[n++] = p->esclavo;                        // direccion
    t[n++] = funcion;                           // codigo de funcion
    t[n++] = (uint8_t)(p->offset >> 8);         // registro inicial (alto primero)
    t[n++] = (uint8_t)(p->offset & 0xFF);

    if (funcion == MB_FC_ESCRIBIR_UNO) {
        t[n++] = (uint8_t)(p->valores[0] >> 8); // valor
        t[n++] = (uint8_t)(p->valores[0] & 0xFF);
    } else {
        t[n++] = (uint8_t)(p->cantidad >> 8);   // cantidad de registros
        t[n++] = (uint8_t)(p->cantidad & 0xFF);
        if (funcion == MB_FC_ESCRIBIR_VARIOS) {
            t[n++] = (uint8_t)(p->cantidad * 2);    // cantidad de bytes
            for (uint16_t i = 0; i < p->cantidad; i++) {
                t[n++] = (uint8_t)(p->valores[i] >> 8);
                t[n++] = (uint8_t)(p->valores[i] & 0xFF);
            }
        }
    }
    n = crc16_modbus_agregar(t, n);
    imprimir_bytes("TX pedido   [dir fc datos CRCl CRCh]", t, n);
}

// La respuesta real no la entrega el stack, asi que se reconstruye a partir de
// los datos recibidos. Tiene que coincidir byte a byte con la captura del bus.
static void imprimir_trama_respuesta(const pedido_t *p, uint8_t funcion, const uint16_t *datos)
{
    uint8_t t[5 + 2 * MB_MAX_REG_POR_PEDIDO];
    size_t n = 0;

    t[n++] = p->esclavo;
    t[n++] = funcion;
    if (funcion == MB_FC_LEER_HOLDING) {
        t[n++] = (uint8_t)(p->cantidad * 2);    // cantidad de bytes de datos
        for (uint16_t i = 0; i < p->cantidad; i++) {
            t[n++] = (uint8_t)(datos[i] >> 8);
            t[n++] = (uint8_t)(datos[i] & 0xFF);
        }
    } else {
        // FC06 responde el eco del pedido; FC16 responde registro + cantidad.
        uint16_t campo = (funcion == MB_FC_ESCRIBIR_UNO) ? p->valores[0] : p->cantidad;
        t[n++] = (uint8_t)(p->offset >> 8);
        t[n++] = (uint8_t)(p->offset & 0xFF);
        t[n++] = (uint8_t)(campo >> 8);
        t[n++] = (uint8_t)(campo & 0xFF);
    }
    n = crc16_modbus_agregar(t, n);
    imprimir_bytes("RX respuesta [reconstruida]           ", t, n);
}

// ---------------------------------------------------------------------------
// Estado de enlace y errores
// ---------------------------------------------------------------------------

static void actualizar_enlace(uint8_t esclavo, esp_err_t err)
{
    if (esclavo > MB_DIR_MAX) {
        return;
    }
    if (err == ESP_OK) {
        if (en_linea[esclavo] != 1) {
            ESP_LOGI(TAG, "Esclavo %u EN LINEA", esclavo);
        }
        en_linea[esclavo] = 1;
    } else if (err == ESP_ERR_TIMEOUT) {
        if (en_linea[esclavo] != 0) {
            ESP_LOGW(TAG, "Esclavo %u FUERA DE LINEA (no responde)", esclavo);
        }
        en_linea[esclavo] = 0;
    }
}

static void informar_error(const char *operacion, uint8_t esclavo, esp_err_t err)
{
    ESP_LOGE(TAG, "%s esclavo %u -> %s [%s]", operacion, esclavo,
             modbus_maestro_describir_error(err), esp_err_to_name(err));
}

static const char *nombre_de(uint16_t offset)
{
    return (offset < MB_CANT_REG_HOLDING) ? nombre_registro[offset] : "(fuera del mapa)";
}

// ---------------------------------------------------------------------------
// Operaciones
// ---------------------------------------------------------------------------

static void hacer_lectura(const pedido_t *p)
{
    uint16_t datos[MB_MAX_REG_POR_PEDIDO] = {0};

    printf("\n>> FC03 Leer esclavo %u, registros %u..%u\n", p->esclavo,
           MB_REG_NUMERO_BASE + p->offset, MB_REG_NUMERO_BASE + p->offset + p->cantidad - 1);
    if (mostrar_tramas) {
        imprimir_trama_pedido(p, MB_FC_LEER_HOLDING);
    }

    esp_err_t err = modbus_maestro_leer(p->esclavo, p->offset, p->cantidad, datos);
    actualizar_enlace(p->esclavo, err);
    if (err != ESP_OK) {
        informar_error("Lectura", p->esclavo, err);
        return;
    }

    if (mostrar_tramas) {
        imprimir_trama_respuesta(p, MB_FC_LEER_HOLDING, datos);
    }
    for (uint16_t i = 0; i < p->cantidad; i++) {
        uint16_t off = p->offset + i;
        printf("   %u  %-24s = %5u  (0x%04X)\n",
               MB_REG_NUMERO_BASE + off, nombre_de(off), datos[i], datos[i]);
    }
}

// Despues de escribir se vuelve a leer para confirmar que el esclavo tomo el
// valor (la respuesta OK solo dice que la trama llego bien).
static void verificar_escritura(const pedido_t *p)
{
    uint16_t leidos[MB_MAX_REG_POR_PEDIDO] = {0};

    esp_err_t err = modbus_maestro_leer(p->esclavo, p->offset, p->cantidad, leidos);
    if (err != ESP_OK) {
        informar_error("Verificacion", p->esclavo, err);
        return;
    }
    for (uint16_t i = 0; i < p->cantidad; i++) {
        if (leidos[i] != p->valores[i]) {
            ESP_LOGW(TAG, "Verificacion: %u vale %u, se escribio %u",
                     MB_REG_NUMERO_BASE + p->offset + i, leidos[i], p->valores[i]);
            return;
        }
    }
    printf("   Verificado por relectura (FC03): OK\n");
}

static void hacer_escritura(const pedido_t *p)
{
    bool    uno     = (p->tipo == PEDIDO_ESCRIBIR_UNO);
    uint8_t funcion = uno ? MB_FC_ESCRIBIR_UNO : MB_FC_ESCRIBIR_VARIOS;

    printf("\n>> FC%02u Escribir esclavo %u:\n", funcion, p->esclavo);
    for (uint16_t i = 0; i < p->cantidad; i++) {
        uint16_t off = p->offset + i;
        printf("   %u  %-24s <- %5u\n", MB_REG_NUMERO_BASE + off, nombre_de(off), p->valores[i]);
    }
    if (mostrar_tramas) {
        imprimir_trama_pedido(p, funcion);
    }

    esp_err_t err = uno
        ? modbus_maestro_escribir_uno(p->esclavo, p->offset, p->valores[0])
        : modbus_maestro_escribir_varios(p->esclavo, p->offset, p->cantidad, p->valores);
    actualizar_enlace(p->esclavo, err);
    if (err != ESP_OK) {
        informar_error("Escritura", p->esclavo, err);
        return;
    }

    if (mostrar_tramas) {
        imprimir_trama_respuesta(p, funcion, NULL);
    }
    printf("   Escritura aceptada por el esclavo\n");
    verificar_escritura(p);
}

// Una consulta del sondeo: salida compacta de una sola linea.
static void hacer_sondeo(uint8_t esclavo)
{
    uint16_t r[SONDEO_CANT_REG] = {0};

    esp_err_t err = modbus_maestro_leer(esclavo, SONDEO_PRIMER_REG, SONDEO_CANT_REG, r);
    actualizar_enlace(esclavo, err);
    if (err != ESP_OK) {
        informar_error("Sondeo", esclavo, err);
        return;
    }
    printf("[SONDEO] Esclavo %u | ADC=%4u  mV=%4u  Contador=%5u  Estado=%u\n",
           esclavo, r[0], r[1], r[2], r[3]);
}

void tarea_modbus(void *parametros)
{
    (void)parametros;

    for (int i = 0; i <= MB_DIR_MAX; i++) {
        en_linea[i] = -1;
    }

    // Estado del sondeo: local de la tarea, solo cambia por pedidos de la cola,
    // asi que no hace falta ningun mutex.
    bool     sondeo_activo  = false;
    uint32_t sondeo_periodo = SONDEO_PERIODO_DEFECTO_MS;
    size_t   proximo        = 0;

    pedido_t p;

    for (;;) {
        // Con sondeo activo, la espera en la cola hace de periodo del sondeo:
        // si no llega ningun pedido en ese tiempo, se consulta al siguiente esclavo.
        TickType_t espera = sondeo_activo ? pdMS_TO_TICKS(sondeo_periodo) : portMAX_DELAY;

        if (xQueueReceive(cola_pedidos, &p, espera) != pdTRUE) {
            hacer_sondeo(esclavos_sondeo[proximo]);
            proximo = (proximo + 1) % CANT_ESCLAVOS_SONDEO;
            continue;
        }

        switch (p.tipo) {

            case PEDIDO_LEER:
                hacer_lectura(&p);
                break;

            case PEDIDO_ESCRIBIR_UNO:
            case PEDIDO_ESCRIBIR_VARIOS:
                hacer_escritura(&p);
                break;

            case PEDIDO_SONDEO:
                sondeo_activo = p.activar;
                if (p.periodo_ms != 0) {
                    sondeo_periodo = p.periodo_ms;
                }
                printf("\n>> Sondeo %s (cada %lu ms, esclavos:", sondeo_activo ? "ACTIVO" : "DETENIDO",
                       (unsigned long)sondeo_periodo);
                for (size_t i = 0; i < CANT_ESCLAVOS_SONDEO; i++) {
                    printf(" %u", esclavos_sondeo[i]);
                }
                printf(")\n");
                break;

            case PEDIDO_TRAMAS:
                mostrar_tramas = p.activar;
                printf("\n>> Mostrar tramas: %s\n", mostrar_tramas ? "SI" : "NO");
                break;

            default:
                ESP_LOGW(TAG, "Pedido desconocido (%d)", p.tipo);
                break;
        }
    }
}
