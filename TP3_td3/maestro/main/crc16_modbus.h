// CRC-16/MODBUS: polinomio 0x8005 (reflejado 0xA001), valor inicial 0xFFFF.
//
// El stack esp-modbus ya calcula y verifica el CRC de cada trama; este modulo
// se usa solo para mostrar por consola las tramas completas (comando "tramas")
// y poder compararlas con lo que captura la PC en el bus.

#pragma once

#include <stddef.h>
#include <stdint.h>

uint16_t crc16_modbus(const uint8_t *datos, size_t largo);

// Agrega el CRC al final de la trama: primero el byte BAJO, despues el ALTO
// (es la unica parte de Modbus que va en little-endian).
// Devuelve el largo total (largo + 2).
size_t crc16_modbus_agregar(uint8_t *trama, size_t largo);
