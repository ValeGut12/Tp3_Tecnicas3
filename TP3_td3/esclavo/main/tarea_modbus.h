// Tarea de comunicacion Modbus del esclavo.

#pragma once

// Espera los accesos del maestro que informa el stack, los registra por consola
// y, cuando el maestro escribe, despierta a la tarea de aplicacion.
// El parametro es el TaskHandle_t de la tarea de aplicacion.
void tarea_modbus(void *parametros);
