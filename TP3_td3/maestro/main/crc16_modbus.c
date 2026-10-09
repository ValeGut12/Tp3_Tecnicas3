// CRC-16/MODBUS bit a bit (sin tabla): lento pero se entiende paso a paso.

#include "crc16_modbus.h"

uint16_t crc16_modbus(const uint8_t *datos, size_t largo)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < largo; i++) {
        crc ^= datos[i];
        // Por cada bit: si el LSB es 1 se desplaza y se aplica el polinomio.
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

size_t crc16_modbus_agregar(uint8_t *trama, size_t largo)
{
    uint16_t crc = crc16_modbus(trama, largo);
    trama[largo]     = (uint8_t)(crc & 0xFF);   // CRC bajo
    trama[largo + 1] = (uint8_t)(crc >> 8);     // CRC alto
    return largo + 2;
}
