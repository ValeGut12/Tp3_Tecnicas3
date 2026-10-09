// Inicializacion del stack Modbus RTU en modo esclavo sobre RS485.

#pragma once

#include "esp_err.h"

// Configura la UART (9600 8E1, RS485 half-duplex), registra el mapa de
// Holding Registers y arranca el stack. Despues de esto el esclavo ya responde.
esp_err_t modbus_esclavo_init(void);

// Handle del controlador, para mbc_slave_check_event / get_param_info.
void *modbus_esclavo_handle(void);
