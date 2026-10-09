// Mapa de registros Modbus de los esclavos.
//
// Este archivo esta copiado igual en maestro/main y esclavo/main: si se cambia
// uno hay que cambiar el otro, porque es el "contrato" entre ambos equipos.
//
// La numeracion 4xxxx es la de la documentacion (Holding Registers). En la
// trama NO viaja 40001, viaja el offset: 40001 -> 0x0000, 40004 -> 0x0003.

#pragma once

#define MB_REG_NUMERO_BASE      40001

// Offsets dentro del area de Holding Registers (indice del arreglo en el esclavo).
#define MB_REG_ADC_CRUDO        0   // 40001  Valor analogico 1: cuentas ADC 0..4095 (GPIO34)
#define MB_REG_ADC_MV           1   // 40002  Valor analogico 2: tension del pote en mV (calibrada)
#define MB_REG_CONTADOR         2   // 40003  Contador, +1 por segundo
#define MB_REG_ESTADO           3   // 40004  Estado: 0 = OFF, 1 = ON -> LED GPIO22
#define MB_REG_SETPOINT         4   // 40005  Setpoint en mV (lo usa el modo 3)
#define MB_REG_TIEMPO           5   // 40006  Periodo de parpadeo en ms (lo usa el modo 2)
#define MB_REG_MODO             6   // 40007  Modo de operacion del LED GPIO23

#define MB_CANT_REG_HOLDING     7

// Valores posibles de 40007.
typedef enum {
    MODO_LED_APAGADO = 0,       // LED2 apagado
    MODO_LED_ENCENDIDO,         // LED2 encendido fijo
    MODO_LED_PARPADEO,          // LED2 parpadea con periodo = 40006 ms
    MODO_LED_SETPOINT,          // LED2 encendido si 40002 (mV) > 40005 (setpoint)
    MODO_LED_CANTIDAD           // marca el final, sirve para validar
} modo_led_t;

// Codigos de funcion Modbus que usa el TP.
#define MB_FC_LEER_HOLDING      0x03    // Read Holding Registers
#define MB_FC_ESCRIBIR_UNO      0x06    // Write Single Holding Register
#define MB_FC_ESCRIBIR_VARIOS   0x10    // Write Multiple Holding Registers (16)
