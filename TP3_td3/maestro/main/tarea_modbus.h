// Tarea de comunicacion Modbus del maestro.

#pragma once

// Unica duenia del bus: atiende los pedidos de la consola (FC03/06/16), hace
// el sondeo ciclico de los esclavos, muestra resultados y detecta errores.
void tarea_modbus(void *parametros);
