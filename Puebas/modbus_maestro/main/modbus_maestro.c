#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "mbcontroller.h"

//Declaracion de variables globales
#define TXD_PIN      GPIO_NUM_17    // Pin de TX     
#define RXD_PIN      GPIO_NUM_16    // Pin de RX
#define DE_RE_PIN    GPIO_NUM_32    // Pin de DE RE
#define MB_PORT_NUM    UART_NUM_2     // Se usa la UART 
#define MB_DEV_SPEED 9600
#define MB_READ_HOLDING_REG   3 

// Direcciones
#define SLAVE_1     1        // Dirección del Esclavo 1

static const char *TAG = "MODBUS_MASTER";
static void *master_handle = NULL;

void modbus_uart_init(void) {   // Configuracion de la UART
    // 1. Configuración de comunicación Modbus
    mb_communication_info_t config = {
        .ser_opts.port = MB_PORT_NUM,           // master communication port number
        .ser_opts.mode = MB_RTU,                // mode of Modbus communication (MB_RTU, MB_ASCII)
        .ser_opts.baudrate = MB_DEV_SPEED,      // baud rate of the port
        .ser_opts.parity = UART_PARITY_EVEN,      // parity option for the port
        .ser_opts.uid = 0,                      // unused for master
        .ser_opts.response_tout_ms = 1000,      // slave response time for master (if = 0, taken from default config)
        .ser_opts.data_bits = UART_DATA_8_BITS, // number of data bits for communication port
        .ser_opts.stop_bits = UART_STOP_BITS_1  // number of stop bits for the communication port
    };

    // 2. Crear el controlador maestro
    esp_err_t err = mbc_master_create_serial(&config, &master_handle);
    if (master_handle == NULL || err != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al inicializar el controlador Modbus maestro");
        return;
    }

    // 3. Asignar los pines físicos y activar modo RS485 half-duplex
    ESP_ERROR_CHECK(uart_set_pin(MB_PORT_NUM, TXD_PIN, RXD_PIN, DE_RE_PIN, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_set_mode(MB_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX));
}

void app_main(void){
    modbus_uart_init();

    // 4. Arrancar el controlador
    ESP_ERROR_CHECK(mbc_master_start(master_handle));

    ESP_LOGI(TAG, "Maestro Modbus inicializado correctamente");

    // 5. Loop: leer el registro 40001 del Esclavo 1 cada 2 segundos
    while (1) {
        uint16_t value = 0;

        mb_param_request_t request = {
            .slave_addr = SLAVE_1,
            .command = MB_READ_HOLDING_REG,            
            .reg_start = 0,     // registro 40001 (offset 0)
            .reg_size = 1,      // leer 1 registro
        };

        esp_err_t read_err = mbc_master_send_request(master_handle, &request, &value);

        if (read_err == ESP_OK) {
            ESP_LOGI(TAG, "Registro 40001 = %d", value);
        } else {
            ESP_LOGE(TAG, "Error al leer esclavo %d: %s", SLAVE_1, esp_err_to_name(read_err));
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}