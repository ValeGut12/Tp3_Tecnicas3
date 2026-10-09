// Tarea 2 (maestro): consola de comandos = aplicacion del maestro.
//
//   teclado (monitor serie, UART0) --> esta tarea --cola_pedidos--> tarea Modbus
//
// Los registros se escriben con la numeracion de la documentacion (40001...);
// la conversion a offset (40001 -> 0) se hace aca.

#include "tarea_consola.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/uart.h"
#include "esp_log.h"

#include "app_config.h"
#include "app_shared.h"

static const char *TAG = "CONSOLA";

#define MAX_ARGUMENTOS  (4 + MB_MAX_REG_POR_PEDIDO)

static void imprimir_ayuda(void)
{
    printf("\n"
           "==================== Maestro Modbus RTU - comandos ====================\n"
           "  leer <esc> <reg> [cant]         FC03  ej: leer 1 40001 4\n"
           "  escribir <esc> <reg> <valor>    FC06  ej: escribir 1 40004 1\n"
           "  escribirm <esc> <reg> <v1> ...  FC16  ej: escribirm 1 40005 2000 500 2\n"
           "  led <esc> on|off                FC06 sobre 40004 (LED1 del esclavo)\n"
           "  sondeo on|off [periodo_ms]      lee 40001-40004 alternando esclavos\n"
           "  tramas on|off                   muestra los bytes de cada trama y su CRC\n"
           "  ayuda                           esta lista\n"
           "  Atajos: r = leer, w = escribir, wm = escribirm\n"
           "  Esclavo: %d..%d. Registros: 40001..40007 (ver mapa en README).\n"
           "  Modo (40007): 0 apagado, 1 encendido, 2 parpadeo, 3 setpoint\n"
           "=======================================================================\n",
           MB_DIR_MIN, MB_DIR_MAX);
}

// Convierte texto a numero y controla el rango. Acepta decimal o hexa (0x...).
static bool a_numero(const char *txt, long min, long max, long *salida)
{
    char *fin = NULL;
    long v = strtol(txt, &fin, 0);
    if (fin == txt || *fin != '\0' || v < min || v > max) {
        return false;
    }
    *salida = v;
    return true;
}

static bool leer_esclavo(const char *txt, uint8_t *esclavo)
{
    long v;
    if (!a_numero(txt, MB_DIR_MIN, MB_DIR_MAX, &v)) {
        printf("Direccion de esclavo invalida '%s' (debe ser %d..%d)\n", txt, MB_DIR_MIN, MB_DIR_MAX);
        return false;
    }
    *esclavo = (uint8_t)v;
    return true;
}

// "40004" -> offset 3.
static bool leer_registro(const char *txt, uint16_t *offset)
{
    long v;
    if (!a_numero(txt, MB_REG_NUMERO_BASE, 49999, &v)) {
        printf("Registro invalido '%s' (usar numeracion 4xxxx, ej. 40001)\n", txt);
        return false;
    }
    *offset = (uint16_t)(v - MB_REG_NUMERO_BASE);
    return true;
}

static bool leer_valor(const char *txt, uint16_t *valor)
{
    long v;
    if (!a_numero(txt, 0, 65535, &v)) {
        printf("Valor invalido '%s' (0..65535)\n", txt);
        return false;
    }
    *valor = (uint16_t)v;
    return true;
}

static bool leer_on_off(const char *txt, bool *activo)
{
    if (strcasecmp(txt, "on") == 0 || strcmp(txt, "1") == 0) {
        *activo = true;
        return true;
    }
    if (strcasecmp(txt, "off") == 0 || strcmp(txt, "0") == 0) {
        *activo = false;
        return true;
    }
    printf("Se esperaba on u off, no '%s'\n", txt);
    return false;
}

static void encolar(const pedido_t *p)
{
    if (xQueueSend(cola_pedidos, p, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Cola de pedidos llena: el comando se descarta");
    }
}

// Interpreta una linea ya separada en palabras.
static void ejecutar(int argc, char **argv)
{
    const char *cmd = argv[0];
    pedido_t p = {0};

    if (strcasecmp(cmd, "ayuda") == 0 || strcasecmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        imprimir_ayuda();
        return;
    }

    // leer <esc> <reg> [cant]
    if (strcasecmp(cmd, "leer") == 0 || strcasecmp(cmd, "r") == 0) {
        if (argc < 3 || argc > 4) {
            printf("Uso: leer <esclavo> <registro> [cantidad]\n");
            return;
        }
        long cant = 1;
        if (!leer_esclavo(argv[1], &p.esclavo) || !leer_registro(argv[2], &p.offset)) {
            return;
        }
        if (argc == 4 && !a_numero(argv[3], 1, MB_MAX_REG_POR_PEDIDO, &cant)) {
            printf("Cantidad invalida (1..%d)\n", MB_MAX_REG_POR_PEDIDO);
            return;
        }
        p.tipo     = PEDIDO_LEER;
        p.cantidad = (uint16_t)cant;
        encolar(&p);
        return;
    }

    // escribir <esc> <reg> <valor>
    if (strcasecmp(cmd, "escribir") == 0 || strcasecmp(cmd, "w") == 0) {
        if (argc != 4) {
            printf("Uso: escribir <esclavo> <registro> <valor>\n");
            return;
        }
        if (!leer_esclavo(argv[1], &p.esclavo) || !leer_registro(argv[2], &p.offset) ||
            !leer_valor(argv[3], &p.valores[0])) {
            return;
        }
        p.tipo     = PEDIDO_ESCRIBIR_UNO;
        p.cantidad = 1;
        encolar(&p);
        return;
    }

    // escribirm <esc> <reg> <v1> [v2 ...]
    if (strcasecmp(cmd, "escribirm") == 0 || strcasecmp(cmd, "wm") == 0) {
        if (argc < 4 || argc > 3 + MB_MAX_REG_POR_PEDIDO) {
            printf("Uso: escribirm <esclavo> <registro> <v1> [v2 ...] (hasta %d valores)\n",
                   MB_MAX_REG_POR_PEDIDO);
            return;
        }
        if (!leer_esclavo(argv[1], &p.esclavo) || !leer_registro(argv[2], &p.offset)) {
            return;
        }
        p.cantidad = (uint16_t)(argc - 3);
        for (uint16_t i = 0; i < p.cantidad; i++) {
            if (!leer_valor(argv[3 + i], &p.valores[i])) {
                return;
            }
        }
        p.tipo = PEDIDO_ESCRIBIR_VARIOS;
        encolar(&p);
        return;
    }

    // led <esc> on|off  ->  FC06 sobre 40004
    if (strcasecmp(cmd, "led") == 0) {
        bool on;
        if (argc != 3) {
            printf("Uso: led <esclavo> on|off\n");
            return;
        }
        if (!leer_esclavo(argv[1], &p.esclavo) || !leer_on_off(argv[2], &on)) {
            return;
        }
        p.tipo       = PEDIDO_ESCRIBIR_UNO;
        p.offset     = MB_REG_ESTADO;
        p.cantidad   = 1;
        p.valores[0] = on ? 1 : 0;
        encolar(&p);
        return;
    }

    // sondeo on|off [periodo_ms]
    if (strcasecmp(cmd, "sondeo") == 0) {
        long periodo = 0;
        if (argc < 2 || argc > 3 || !leer_on_off(argv[1], &p.activar)) {
            printf("Uso: sondeo on|off [periodo_ms]\n");
            return;
        }
        if (argc == 3 && !a_numero(argv[2], SONDEO_PERIODO_MIN_MS, 60000, &periodo)) {
            printf("Periodo invalido (%d..60000 ms)\n", SONDEO_PERIODO_MIN_MS);
            return;
        }
        p.tipo       = PEDIDO_SONDEO;
        p.periodo_ms = (uint32_t)periodo;
        encolar(&p);
        return;
    }

    // tramas on|off
    if (strcasecmp(cmd, "tramas") == 0) {
        if (argc != 2 || !leer_on_off(argv[1], &p.activar)) {
            printf("Uso: tramas on|off\n");
            return;
        }
        p.tipo = PEDIDO_TRAMAS;
        encolar(&p);
        return;
    }

    printf("Comando desconocido '%s'. Escribir 'ayuda'.\n", cmd);
}

static void procesar_linea(char *linea)
{
    char *argv[MAX_ARGUMENTOS];
    int   argc = 0;
    char *resto = NULL;

    for (char *tok = strtok_r(linea, " \t", &resto);
         tok != NULL && argc < MAX_ARGUMENTOS;
         tok = strtok_r(NULL, " \t", &resto)) {
        argv[argc++] = tok;
    }
    if (argc > 0) {
        ejecutar(argc, argv);
    }
}

void tarea_consola(void *parametros)
{
    (void)parametros;

    // Driver de UART0 solo para recibir (buffer RX). printf sigue saliendo por
    // el mismo puerto como siempre.
    if (!uart_is_driver_installed(CONSOLA_UART)) {
        ESP_ERROR_CHECK(uart_driver_install(CONSOLA_UART, 256, 0, 0, NULL, 0));
    }

    imprimir_ayuda();
    printf("> ");
    fflush(stdout);

    char   linea[CONSOLA_LARGO_LINEA];
    size_t largo = 0;

    for (;;) {
        uint8_t c;
        // Bloquea hasta que llegue un caracter: la tarea no consume CPU mientras espera.
        if (uart_read_bytes(CONSOLA_UART, &c, 1, portMAX_DELAY) != 1) {
            continue;
        }

        if (c == '\r' || c == '\n') {
            if (largo == 0) {
                continue;       // CR+LF o Enter vacio
            }
            printf("\n");
            linea[largo] = '\0';
            procesar_linea(linea);
            largo = 0;
            // Pausa corta para que la respuesta de la tarea Modbus salga antes del prompt.
            vTaskDelay(pdMS_TO_TICKS(50));
            printf("> ");
            fflush(stdout);
        } else if (c == 0x08 || c == 0x7F) {
            // Backspace: borra el ultimo caracter tambien en pantalla.
            if (largo > 0) {
                largo--;
                printf("\b \b");
                fflush(stdout);
            }
        } else if (isprint(c) && largo < sizeof(linea) - 1) {
            linea[largo++] = (char)c;
            putchar(c);         // el monitor no hace eco local
            fflush(stdout);
        }
    }
}
