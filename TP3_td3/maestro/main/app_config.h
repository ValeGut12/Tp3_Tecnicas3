// Maestro Modbus RTU: pines, parametros del bus, esclavos, periodos,
// prioridades y stacks. Todo lo configurable esta aca.

#pragma once

#include "driver/gpio.h"
#include "driver/uart.h"

#include "mapa_modbus.h"

// --- RS485: UART2 -> conversor de nivel -> MAX485 ---
//
//   GPIO17 (TX2)  -> DI
//   GPIO16 (RX2)  <- RO
//   GPIO32        -> DE y /RE unidos (RTS de la UART en modo RS485 half-duplex)
#define MB_UART_PUERTO          UART_NUM_2
#define MB_PIN_TX               GPIO_NUM_17
#define MB_PIN_RX               GPIO_NUM_16
#define MB_PIN_DE_RE            GPIO_NUM_32

// 9600 8E1: tiene que ser igual en TODOS los nodos del bus.
#define MB_BAUDIOS              9600
#define MB_PARIDAD              UART_PARITY_EVEN

// Cuanto espera el maestro una respuesta antes de declarar TIMEOUT.
// A 9600 bps una respuesta de 4 registros (13 bytes x 11 bits) tarda ~15 ms,
// asi que 1 s sobra; se deja holgado para que la demo sea clara.
#define MB_TIMEOUT_RESPUESTA_MS 1000

// --- Direcciones ---
#define MB_DIR_MIN              1
#define MB_DIR_MAX              247

// Esclavos que recorre el sondeo ciclico (se consultan alternadamente).
// Al sumar el tercer ESP32 alcanza con agregar su direccion aca.
#define MB_ESCLAVOS_SONDEO      { 1, 2 }

// --- Sondeo ciclico (comando "sondeo") ---
#define SONDEO_PERIODO_DEFECTO_MS   1000    // tiempo entre consultas
#define SONDEO_PERIODO_MIN_MS       100
#define SONDEO_PRIMER_REG           MB_REG_ADC_CRUDO    // lee 40001..40004
#define SONDEO_CANT_REG             4

// Maximo de registros por lectura (FC03) o escritura multiple (FC16) desde la consola.
#define MB_MAX_REG_POR_PEDIDO   16

// --- Consola (monitor serie por USB = UART0) ---
#define CONSOLA_UART            UART_NUM_0
#define CONSOLA_LARGO_LINEA     128

// --- Colas ---
#define COLA_PEDIDOS_LARGO      8

// --- Prioridades: numero mas alto = mas prioridad ---
#define PRIORIDAD_TAREA_MODBUS  5
#define PRIORIDAD_TAREA_CONSOLA 3

// --- Stacks en bytes ---
#define STACK_TAREA_MODBUS      4096
#define STACK_TAREA_CONSOLA     4096
