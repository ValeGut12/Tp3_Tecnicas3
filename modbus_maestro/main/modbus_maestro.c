#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"

//Declaracion de variables globales
#define TXD_PIN      GPIO_NUM_17    // Pin de TX     
#define RXD_PIN      GPIO_NUM_16    // Pin de RX
#define DE_RE_PIN    GPIO_NUM_32    //
#define UART_PORT    UART_NUM_2     //

void modbus_uart_init(void) {
    gpio_set_direction(DE_RE_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DE_RE_PIN, 0); 
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_PORT, 256, 256, 0, NULL, 0);
}

void modbus_send(const uint8_t *data, size_t len) {
    gpio_set_level(DE_RE_PIN, 1);
    uart_write_bytes(UART_PORT, (const char *)data, len);
    uart_wait_tx_done(UART_PORT, pdMS_TO_TICKS(100));
    gpio_set_level(DE_RE_PIN, 0);
}

void app_main(void) {
    modbus_uart_init();

    const char *msg = "HOLA MUNDO\n";

    while (1) {
        printf("Enviando por RS485: %s", msg);
        modbus_send((const uint8_t *)msg, strlen(msg));
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}