// Tarea de consola (aplicacion) del maestro.

#pragma once

// Lee comandos del monitor serie (UART0), los valida y los manda como pedidos
// a la tarea Modbus por cola_pedidos. Escribir "ayuda" para ver la lista.
void tarea_consola(void *parametros);
