// Capa Modbus del maestro: inicializacion y las tres funciones del TP.
//
// Solo la tarea Modbus llama a estas funciones: el bus es half-duplex y el
// maestro tiene que mandar un pedido por vez y esperar su respuesta.

#pragma once

#include <stdint.h>

#include "esp_err.h"

// Configura la UART (9600 8E1, RS485 half-duplex) y arranca el stack maestro.
esp_err_t modbus_maestro_init(void);

// FC03 - Read Holding Registers. 'destino' debe tener lugar para 'cantidad'.
esp_err_t modbus_maestro_leer(uint8_t esclavo, uint16_t offset, uint16_t cantidad,
                              uint16_t *destino);

// FC06 - Write Single Holding Register.
esp_err_t modbus_maestro_escribir_uno(uint8_t esclavo, uint16_t offset, uint16_t valor);

// FC16 - Write Multiple Holding Registers.
esp_err_t modbus_maestro_escribir_varios(uint8_t esclavo, uint16_t offset, uint16_t cantidad,
                                         const uint16_t *valores);

// Texto explicativo del error, para mostrar por consola.
const char *modbus_maestro_describir_error(esp_err_t err);
