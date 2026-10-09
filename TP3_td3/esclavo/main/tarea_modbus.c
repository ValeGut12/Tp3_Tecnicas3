// Tarea 1 (esclavo): comunicacion Modbus.
//
// La recepcion de la trama, la verificacion del CRC, el filtrado por direccion
// y el armado de la respuesta los hace el stack esp-modbus en su propia tarea.
// Esta tarea se entera de cada acceso ya validado y hace dos cosas:
//   - lo muestra por consola (sirve para las capturas del informe);
//   - si fue una escritura, le avisa a la tarea de aplicacion con una
//     notificacion directa para que actualice las salidas sin esperar.

#include "tarea_modbus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "mbcontroller.h"

#include "app_config.h"
#include "modbus_esclavo.h"

static const char *TAG = "TAREA_MODBUS";

// Solo interesan los Holding Registers: es el unico tipo que tiene el mapa.
#define EVENTOS_HOLDING         (MB_EVENT_HOLDING_REG_RD | MB_EVENT_HOLDING_REG_WR)

// Cuanto se espera el detalle del acceso despues de recibir el evento.
#define TIMEOUT_INFO_ACCESO     10

void tarea_modbus(void *parametros)
{
    TaskHandle_t tarea_aplicacion = (TaskHandle_t)parametros;
    void *handle = modbus_esclavo_handle();

    mb_param_info_t info;

    for (;;) {
        // Bloquea hasta que el maestro lea o escriba algun Holding Register de
        // ESTE esclavo. Las tramas para otras direcciones ni llegan aca.
        mb_event_group_t evento = mbc_slave_check_event(handle, EVENTOS_HOLDING);

        if (mbc_slave_get_param_info(handle, &info, TIMEOUT_INFO_ACCESO) != ESP_OK) {
            continue;
        }

        uint32_t primero = MB_REG_NUMERO_BASE + info.mb_offset;
        uint32_t ultimo  = primero + info.size - 1;

        if (evento & MB_EVENT_HOLDING_REG_WR) {
            ESP_LOGI(TAG, "Maestro ESCRIBIO %lu..%lu (%u reg)",
                     (unsigned long)primero, (unsigned long)ultimo, (unsigned)info.size);

            // La aplicacion aplica los cambios a las salidas de inmediato.
            xTaskNotifyGive(tarea_aplicacion);
        } else if (evento & MB_EVENT_HOLDING_REG_RD) {
            ESP_LOGI(TAG, "Maestro LEYO    %lu..%lu (%u reg)",
                     (unsigned long)primero, (unsigned long)ultimo, (unsigned)info.size);
        }
    }
}
