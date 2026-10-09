// Tarea 2 (esclavo): adquisicion.
//
//   ADC (GPIO34) --> esta tarea --> 40001 cuentas, 40002 mV
//   reloj 1 s    --> esta tarea --> 40003 contador

#include "tarea_adquisicion.h"

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_log.h"

#include "app_config.h"
#include "registros.h"

static const char *TAG = "TAREA_ADQ";

static adc_oneshot_unit_handle_t unidad_adc = NULL;
static adc_cali_handle_t         cali_adc   = NULL;
static bool                      hay_calibracion = false;

static esp_err_t configurar_adc(void)
{
    adc_oneshot_unit_init_cfg_t cfg_unidad = {
        .unit_id = ADC_UNIDAD,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&cfg_unidad, &unidad_adc), TAG,
                        "no se pudo crear la unidad ADC");

    adc_oneshot_chan_cfg_t cfg_canal = {
        .atten    = ADC_ATENUACION,
        .bitwidth = ADC_ANCHO_BITS,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(unidad_adc, ADC_CANAL_POTE, &cfg_canal),
                        TAG, "no se pudo configurar el canal del pote");

    // En el ESP32 el esquema disponible es "line fitting" con los datos del eFuse.
    // Si el chip no trae calibracion, 40002 queda en una conversion aproximada.
    adc_cali_line_fitting_config_t cfg_cali = {
        .unit_id  = ADC_UNIDAD,
        .atten    = ADC_ATENUACION,
        .bitwidth = ADC_ANCHO_BITS,
    };
    hay_calibracion = (adc_cali_create_scheme_line_fitting(&cfg_cali, &cali_adc) == ESP_OK);
    if (!hay_calibracion) {
        ESP_LOGW(TAG, "ADC sin calibracion de fabrica: los mV seran aproximados");
    }
    return ESP_OK;
}

// Promedio de varias conversiones para que el valor no baile tanto.
static int leer_pote_crudo(void)
{
    int suma = 0;
    int validas = 0;
    for (int i = 0; i < ADC_MUESTRAS_PROMEDIO; i++) {
        int crudo;
        if (adc_oneshot_read(unidad_adc, ADC_CANAL_POTE, &crudo) == ESP_OK) {
            suma += crudo;
            validas++;
        }
    }
    return (validas > 0) ? (suma / validas) : 0;
}

static int crudo_a_mv(int crudo)
{
    int mv = 0;
    if (hay_calibracion && adc_cali_raw_to_voltage(cali_adc, crudo, &mv) == ESP_OK) {
        return mv;
    }
    // Aproximacion lineal: 4095 cuentas ~ 3100 mV con atenuacion de 12 dB.
    return (crudo * 3100) / 4095;
}

void tarea_adquisicion(void *parametros)
{
    (void)parametros;

    if (configurar_adc() != ESP_OK) {
        ESP_LOGE(TAG, "ADC no disponible: 40001 y 40002 quedan en 0");
    }

    const uint32_t ciclos_por_cuenta = PERIODO_CONTADOR_MS / PERIODO_ADQUISICION_MS;
    uint32_t ciclo = 0;

    // vTaskDelayUntil mantiene el periodo fijo aunque la lectura tarde.
    TickType_t ultimo_despertar = xTaskGetTickCount();

    for (;;) {
        if (unidad_adc != NULL) {
            int crudo = leer_pote_crudo();
            uint16_t valores[2] = {
                (uint16_t)crudo,
                (uint16_t)crudo_a_mv(crudo),
            };
            // 40001 y 40002 se escriben juntas, bajo un mismo lock, para que el
            // maestro nunca lea cuentas de una muestra y mV de otra.
            registros_escribir_bloque(MB_REG_ADC_CRUDO, 2, valores);
        }

        if (++ciclo >= ciclos_por_cuenta) {
            ciclo = 0;
            registros_incrementar_contador();
        }

        vTaskDelayUntil(&ultimo_despertar, pdMS_TO_TICKS(PERIODO_ADQUISICION_MS));
    }
}
