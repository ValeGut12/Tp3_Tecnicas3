// Tarea 3 (esclavo): aplicacion.
//
//   Maestro --FC06--> 40004        --> esta tarea --> LED1 (GPIO22)
//   Maestro --FC16--> 40005..40007 --> esta tarea --> LED2 (GPIO23)
//
// Se despierta por dos motivos:
//   - notificacion de la tarea Modbus: el maestro acaba de escribir, se aplica
//     el cambio al instante y se informa por consola;
//   - cada 10 ms: hace falta para el parpadeo (modo 2) y para seguir al pote
//     contra el setpoint (modo 3), que cambian sin que nadie escriba.

#include "tarea_aplicacion.h"

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "app_config.h"
#include "registros.h"

static const char *TAG = "TAREA_APP";

static const char *nombre_modo[MODO_LED_CANTIDAD] = {
    "APAGADO",
    "ENCENDIDO",
    "PARPADEO",
    "SETPOINT",
};

static void configurar_leds(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << LED_ESTADO_GPIO) | (1ULL << LED_MODO_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    gpio_set_level(LED_ESTADO_GPIO, 0);
    gpio_set_level(LED_MODO_GPIO, 0);
}

static void informar_configuracion(const uint16_t *r)
{
    ESP_LOGI(TAG, "Estado(40004)=%u -> LED1 %s",
             r[MB_REG_ESTADO], r[MB_REG_ESTADO] ? "ON" : "OFF");

    if (r[MB_REG_MODO] < MODO_LED_CANTIDAD) {
        ESP_LOGI(TAG, "Setpoint(40005)=%u mV  Tiempo(40006)=%u ms  Modo(40007)=%u -> LED2 %s",
                 r[MB_REG_SETPOINT], r[MB_REG_TIEMPO], r[MB_REG_MODO],
                 nombre_modo[r[MB_REG_MODO]]);
    } else {
        ESP_LOGW(TAG, "Modo(40007)=%u invalido (0..%d): LED2 apagado",
                 r[MB_REG_MODO], MODO_LED_CANTIDAD - 1);
    }
}

void tarea_aplicacion(void *parametros)
{
    (void)parametros;

    configurar_leds();

    bool       led_parpadeo  = false;
    TickType_t ultimo_cambio = xTaskGetTickCount();

    // Se muestra la configuracion inicial una vez.
    bool mostrar = true;

    for (;;) {
        // Copia local de todo el mapa bajo un solo lock: se trabaja sobre una
        // foto coherente aunque el maestro escriba en el medio.
        uint16_t r[MB_CANT_REG_HOLDING];
        registros_leer_bloque(0, MB_CANT_REG_HOLDING, r);

        if (mostrar) {
            informar_configuracion(r);
            mostrar = false;
        }

        // LED1: cualquier valor distinto de 0 se toma como ON.
        gpio_set_level(LED_ESTADO_GPIO, r[MB_REG_ESTADO] != 0);

        // LED2 segun el modo.
        bool led_modo = false;
        switch (r[MB_REG_MODO]) {

            case MODO_LED_APAGADO:
                led_modo = false;
                break;

            case MODO_LED_ENCENDIDO:
                led_modo = true;
                break;

            case MODO_LED_PARPADEO: {
                // 40006 es el periodo completo: se conmuta cada medio periodo.
                uint32_t periodo = r[MB_REG_TIEMPO];
                if (periodo < TIEMPO_PARPADEO_MIN_MS) {
                    periodo = TIEMPO_PARPADEO_MIN_MS;
                }
                TickType_t ahora = xTaskGetTickCount();
                if ((ahora - ultimo_cambio) >= pdMS_TO_TICKS(periodo / 2)) {
                    led_parpadeo  = !led_parpadeo;
                    ultimo_cambio = ahora;
                }
                led_modo = led_parpadeo;
                break;
            }

            case MODO_LED_SETPOINT:
                led_modo = (r[MB_REG_ADC_MV] > r[MB_REG_SETPOINT]);
                break;

            default:
                led_modo = false;   // modo invalido: salida en estado seguro
                break;
        }
        gpio_set_level(LED_MODO_GPIO, led_modo);

        // Espera una escritura del maestro o, como maximo, 10 ms.
        // pdTRUE: si llegaron varias notificaciones juntas cuentan como una.
        mostrar = (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(PERIODO_APLICACION_MS)) > 0);
    }
}
