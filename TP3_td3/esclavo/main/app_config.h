// Esclavo Modbus RTU: pines, direccion, periodos, prioridades y stacks.
// Todo lo configurable esta aca.

#pragma once

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_adc/adc_oneshot.h"

#include "mapa_modbus.h"

// --- Direccion Modbus propia ---
//
// CAMBIAR ANTES DE GRABAR CADA PLACA: 1 para el Esclavo 1, 2 para el Esclavo 2...
// Rango valido 1..247 (0 es broadcast y 248..255 estan reservadas).
#define MB_DIRECCION_ESCLAVO    1

// --- RS485: UART2 -> conversor de nivel -> MAX485 ---
//
//   GPIO17 (TX2)  -> DI  (lo que el ESP32 transmite al bus)
//   GPIO16 (RX2)  <- RO  (lo que el MAX485 recibe del bus)
//   GPIO32        -> DE y /RE unidos. Lo maneja la UART como senal RTS:
//                    1 = transmitir, 0 = recibir.
#define MB_UART_PUERTO          UART_NUM_2
#define MB_PIN_TX               GPIO_NUM_17
#define MB_PIN_RX               GPIO_NUM_16
#define MB_PIN_DE_RE            GPIO_NUM_32

// 9600 8E1: tiene que ser igual en TODOS los nodos del bus.
#define MB_BAUDIOS              9600
#define MB_PARIDAD              UART_PARITY_EVEN

// --- Salidas ---
#define LED_ESTADO_GPIO         GPIO_NUM_22     // LED1, lo comanda 40004 (FC06)
#define LED_MODO_GPIO           GPIO_NUM_23     // LED2, lo comandan 40005..40007 (FC16)

// --- ADC: potenciometro en GPIO34 (ADC1 canal 6) ---
//
// El pote se alimenta con 3V3, NO con 5V: el pin del ESP32 no tolera mas de 3,3 V.
#define ADC_UNIDAD              ADC_UNIT_1
#define ADC_CANAL_POTE          ADC_CHANNEL_6   // GPIO34
#define ADC_ATENUACION          ADC_ATTEN_DB_12 // rango util ~0..3100 mV
#define ADC_ANCHO_BITS          ADC_BITWIDTH_12
#define ADC_MUESTRAS_PROMEDIO   8               // promedio simple para bajar el ruido

// --- Periodos ---
#define PERIODO_ADQUISICION_MS  100             // refresco de 40001/40002
#define PERIODO_CONTADOR_MS     1000            // 40003 sube 1 por segundo
#define PERIODO_APLICACION_MS   10              // resolucion del parpadeo del LED2

// El parpadeo no baja de este periodo aunque el maestro escriba menos en 40006.
#define TIEMPO_PARPADEO_MIN_MS  20

// --- Valores iniciales de los registros de control ---
#define ESTADO_INICIAL          0               // LED1 apagado
#define SETPOINT_INICIAL_MV     1650            // mitad del recorrido del pote
#define TIEMPO_INICIAL_MS       500
#define MODO_INICIAL            MODO_LED_APAGADO

// --- Prioridades: numero mas alto = mas prioridad ---
//
// Modbus arriba de todo: si la respuesta se demora, el maestro declara timeout.
#define PRIORIDAD_TAREA_MODBUS      5
#define PRIORIDAD_TAREA_APLICACION  4
#define PRIORIDAD_TAREA_ADQUISICION 3

// --- Stacks en bytes ---
#define STACK_TAREA_MODBUS          4096
#define STACK_TAREA_APLICACION      3072
#define STACK_TAREA_ADQUISICION     3072
