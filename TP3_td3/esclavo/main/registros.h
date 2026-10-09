// Mapa de Holding Registers del esclavo (40001..40007).
//
// El arreglo lo leen y escriben dos "actores" a la vez:
//   - el stack Modbus, cuando llega una trama del maestro (en su propia tarea);
//   - las tareas de adquisicion y aplicacion.
// Por eso todo acceso desde el programa pasa por estas funciones, que toman el
// lock del controlador Modbus. Asi el stack nunca responde un bloque a medio
// actualizar (por ejemplo 40001 nuevo y 40002 viejo).

#pragma once

#include <stdint.h>

#include "mapa_modbus.h"

// Carga los valores iniciales. Se llama antes de arrancar el stack Modbus.
void registros_valores_iniciales(void);

// Direccion y tamano (en bytes) del arreglo, para registrarlo en el stack.
uint16_t *registros_area(void);
uint32_t  registros_area_bytes(void);

// A partir de aca los accesos quedan protegidos con el lock del controlador.
void registros_asignar_controlador(void *handle_modbus);

uint16_t registros_leer(uint16_t offset);
void     registros_escribir(uint16_t offset, uint16_t valor);

// Copia/escribe varios registros consecutivos bajo un mismo lock.
void registros_leer_bloque(uint16_t offset, uint16_t cantidad, uint16_t *destino);
void registros_escribir_bloque(uint16_t offset, uint16_t cantidad, const uint16_t *valores);

// Incrementa 40003 (lectura-modificacion-escritura bajo el mismo lock).
void registros_incrementar_contador(void);
