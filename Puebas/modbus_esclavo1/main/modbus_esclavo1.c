#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "mbcontroller.h"

// Pines físicos (mismos números, pero en OTRO ESP32 físico)
#define TXD_PIN        GPIO_NUM_17
#define RXD_PIN        GPIO_NUM_16
#define DE_RE_PIN      GPIO_NUM_32

#define MB_PORT_NUM    UART_NUM_2
#define MB_DEV_SPEED   9600
#define MY_SLAVE_ADDR  1        // <-- ESTA es la dirección propia del esclavo

#define MB_REG_HOLD_CNT 4       // 4 registros: 40001 a 40004

static const char *TAG = "MODBUS_SLAVE_1";
static void *slave_handle = NULL;

// Acá viven los valores reales de los registros 40001-40004
static uint16_t holding_reg_area[MB_REG_HOLD_CNT] = {0};

void modbus_slave_init(void)
{
    mb_communication_info_t config = {
        .ser_opts.port = MB_PORT_NUM,
        .ser_opts.mode = MB_RTU,
        .ser_opts.baudrate = MB_DEV_SPEED,
        .ser_opts.parity = UART_PARITY_EVEN,
        .ser_opts.uid = MY_SLAVE_ADDR,   // acá SÍ se define la dirección propia
        .ser_opts.data_bits = UART_DATA_8_BITS,
        .ser_opts.stop_bits = UART_STOP_BITS_1,
    };

    esp_err_t err = mbc_slave_create_serial(&config, &slave_handle);
    if (slave_handle == NULL || err != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al inicializar el controlador Modbus esclavo");
        return;
    }

    ESP_ERROR_CHECK(uart_set_pin(MB_PORT_NUM, TXD_PIN, RXD_PIN, DE_RE_PIN, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_set_mode(MB_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX));

    // Le decimos a la librería: "esta zona de memoria representa los holding registers"
    mb_register_area_descriptor_t reg_area = {
        .type = MB_PARAM_HOLDING,
        .start_offset = 0,                     // registro 40001 = offset 0
        .address = (void *)&holding_reg_area[0],
        .size = sizeof(holding_reg_area),
        .access = MB_ACCESS_RW,
    };
    ESP_ERROR_CHECK(mbc_slave_set_descriptor(slave_handle, reg_area));

    ESP_ERROR_CHECK(mbc_slave_start(slave_handle));
}

void app_main(void)
{
    modbus_slave_init();
    ESP_LOGI(TAG, "Esclavo Modbus (dirección %d) inicializado", MY_SLAVE_ADDR);

    // Cargamos valores de prueba en los registros, para que el Maestro tenga algo que leer
    holding_reg_area[0] = 1234;  // 40001
    holding_reg_area[1] = 5678;  // 40002
    holding_reg_area[2] = 0;     // 40003 - contador
    holding_reg_area[3] = 0;     // 40004 - estado

    while (1) {
        // El esclavo no necesita "hacer" nada activamente en el loop:
        // la librería atiende las peticiones del Maestro en segundo plano.
        // Acá solo simulamos un contador que se actualiza solo, para tener algo dinámico.
        holding_reg_area[2]++;
        ESP_LOGI(TAG, "Contador (40003) = %d", holding_reg_area[2]);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}