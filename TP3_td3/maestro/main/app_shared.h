// Cola y tipo de mensaje que se intercambian las tareas del maestro.
//
//   CONSOLA --cola_pedidos--> MODBUS --> bus RS485
//
// La consola nunca toca el bus: solo arma pedidos. Asi hay un unico duenio del
// bus (la tarea Modbus) y los pedidos se atienden de a uno, en orden.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "app_config.h"

typedef enum {
    PEDIDO_LEER = 0,        // FC03
    PEDIDO_ESCRIBIR_UNO,    // FC06
    PEDIDO_ESCRIBIR_VARIOS, // FC16
    PEDIDO_SONDEO,          // activar/desactivar la consulta ciclica
    PEDIDO_TRAMAS,          // mostrar/ocultar los bytes de cada trama
} tipo_pedido_t;

typedef struct {
    tipo_pedido_t tipo;

    // Para PEDIDO_LEER / ESCRIBIR_*
    uint8_t  esclavo;
    uint16_t offset;                            // 40001 -> 0
    uint16_t cantidad;
    uint16_t valores[MB_MAX_REG_POR_PEDIDO];    // solo en escrituras

    // Para PEDIDO_SONDEO / PEDIDO_TRAMAS
    bool     activar;
    uint32_t periodo_ms;                        // solo en PEDIDO_SONDEO (0 = no cambiar)
} pedido_t;

extern QueueHandle_t cola_pedidos;

// Hay que llamarla antes de crear las tareas.
bool app_shared_crear_objetos(void);
