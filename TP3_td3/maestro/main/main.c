// TP3 - Tecnicas Digitales III - Maestro Modbus RTU sobre RS485.
// Aca solo se arma el sistema: cola, stack Modbus y tareas. La logica de cada
// tarea vive en su propio archivo.

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "app_config.h"
#include "app_shared.h"
#include "modbus_maestro.h"
#include "tarea_consola.h"
#include "tarea_modbus.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    // La cola va primero: las dos tareas la usan.
    if (!app_shared_crear_objetos()) {
        ESP_LOGE(TAG, "No se pudo crear la cola de pedidos. Se aborta.");
        return;
    }

    if (modbus_maestro_init() != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo iniciar Modbus. Se aborta.");
        return;
    }

    xTaskCreate(tarea_modbus, "tarea_modbus", STACK_TAREA_MODBUS, NULL,
                PRIORIDAD_TAREA_MODBUS, NULL);
    xTaskCreate(tarea_consola, "tarea_consola", STACK_TAREA_CONSOLA, NULL,
                PRIORIDAD_TAREA_CONSOLA, NULL);

    ESP_LOGI(TAG, "Maestro en marcha (2 tareas). app_main termina.");
}
