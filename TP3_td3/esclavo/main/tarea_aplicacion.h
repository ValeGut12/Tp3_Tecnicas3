// Tarea de aplicacion del esclavo.

#pragma once

// Usa los registros que escribe el maestro para manejar las salidas:
//   40004        -> LED1 (GPIO22) encendido / apagado
//   40005..40007 -> LED2 (GPIO23) segun el modo de operacion
void tarea_aplicacion(void *parametros);
