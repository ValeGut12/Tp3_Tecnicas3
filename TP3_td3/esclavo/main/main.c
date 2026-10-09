// TP3 - Tecnicas Digitales III - Esclavo Modbus RTU sobre RS485.
// Aca solo se arma el sistema: mapa de registros, stack Modbus y tareas.
// La logica de cada tarea vive en su propio archivo.
//
// La direccion del esclavo se cambia en app_config.h (MB_DIRECCION_ESCLAVO).

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "app_config.h"
#include "modbus_esclavo.h"
#include "registros.h"
#include "tarea_adquisicion.h"
#include "tarea_aplicacion.h"
#include "tarea_modbus.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    // Los registros tienen que tener valores validos antes de que el maestro
    // pueda leerlos, o sea antes de arrancar el stack.
    registros_valores_iniciales();

    if (modbus_esclavo_init() != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo iniciar Modbus. Se aborta.");
        return;
    }

    // La aplicacion va primero porque la tarea Modbus necesita su handle para
    // notificarle las escrituras del maestro.
    TaskHandle_t handle_aplicacion = NULL;
    xTaskCreate(tarea_aplicacion, "tarea_app", STACK_TAREA_APLICACION, NULL,
                PRIORIDAD_TAREA_APLICACION, &handle_aplicacion);

    xTaskCreate(tarea_modbus, "tarea_modbus", STACK_TAREA_MODBUS, handle_aplicacion,
                PRIORIDAD_TAREA_MODBUS, NULL);

    xTaskCreate(tarea_adquisicion, "tarea_adq", STACK_TAREA_ADQUISICION, NULL,
                PRIORIDAD_TAREA_ADQUISICION, NULL);

    ESP_LOGI(TAG, "Esclavo %d en marcha (3 tareas). app_main termina.", MB_DIRECCION_ESCLAVO);
}
