# Contexto para redactar el informe del TP3 (Técnicas Digitales III, UTN)

Este archivo resume el trabajo hecho y los resultados medidos, como insumo para escribir el informe en LaTeX. Se sube junto con: la consigna (PDF), `README.md`, el código de `maestro/main` y `esclavo/main`, `pc/modbus_pc.py` y la plantilla LaTeX.

## 1. Sistema implementado
- Red Modbus RTU sobre RS485, 9600 8E1, con un ESP32 maestro, un ESP32 esclavo (dirección 1; el código ya está listo para el esclavo 2) y un adaptador USB-RS485 (CH340) en la PC.
- Solo ESP-IDF v6.0.2 y FreeRTOS, con la librería `espressif/esp-modbus` 2.1.3. No se usa nada de Arduino.
- Cada nodo tiene: ESP32 DevKit V1 → conversor de nivel 3,3/5 V → módulo MAX485 (5 V).
  - GPIO17 (TX2) → DI
  - RO → GPIO16 (RX2)
  - GPIO32 → DE y /RE unidos
- **DE/RE por hardware:** GPIO32 está configurado como RTS de la UART2, en modo `UART_MODE_RS485_HALF_DUPLEX`. La UART lo pone en 1 durante la transmisión y lo baja al terminar el último bit de stop.
- Salidas del esclavo: LED1 en GPIO22 y LED2 en GPIO23, cada uno con 1 kΩ. Potenciómetro en GPIO34 (ADC1_CH6), alimentado con 3V3.
- Dirección del esclavo: `MB_DIRECCION_ESCLAVO` en `esclavo/main/app_config.h`.

## 2. Mapa de registros (Holding Registers)
| Reg | Offset | Contenido | Escribe |
|---|---|---|---|
| 40001 | 0 | ADC crudo, 0..4095 (GPIO34, promedio de 8 muestras) | tarea de adquisición, cada 100 ms |
| 40002 | 1 | Tensión del pote en mV (calibración line-fitting) | tarea de adquisición |
| 40003 | 2 | Contador, +1 por segundo | tarea de adquisición |
| 40004 | 3 | Estado 0/1 → LED1 | maestro, con FC06 |
| 40005 | 4 | Setpoint en mV | maestro, con FC16 |
| 40006 | 5 | Tiempo: período de parpadeo en ms | maestro, con FC16 |
| 40007 | 6 | Modo de LED2: 0 apagado, 1 encendido, 2 parpadeo, 3 encendido si 40002 > 40005 | maestro, con FC16 |

El valor analógico 2 es la tensión en mV porque el ESP32 clásico no tiene sensor de temperatura soportado en ESP-IDF.

## 3. Tareas FreeRTOS
**Esclavo:**
- `tarea_modbus` (prioridad 5): se bloquea en `mbc_slave_check_event()`, registra cada lectura y escritura del maestro, y en las escrituras notifica a la aplicación con `xTaskNotifyGive`.
- `tarea_aplicacion` (prioridad 4): espera con `ulTaskNotifyTake` y timeout de 10 ms. Maneja el LED1 según 40004 y el LED2 según el modo.
- `tarea_adquisicion` (prioridad 3): usa `vTaskDelayUntil` con período de 100 ms. Lee el ADC y actualiza el contador cada 1 s.
- Todo acceso al mapa de registros pasa por `registros.c`, que usa `mbc_slave_lock()/unlock()`. Así el stack Modbus nunca responde con datos a medio actualizar.
- La recepción, el CRC, el filtrado por dirección y la respuesta los resuelve el stack esp-modbus en su propia tarea.

**Maestro:**
- `tarea_modbus` (prioridad 5): es la única que usa el bus. Atiende los pedidos de una cola, hace el sondeo cíclico (el timeout de la cola funciona como período), detecta errores y avisa cuando un esclavo pasa a EN LÍNEA o FUERA DE LÍNEA. Después de cada escritura vuelve a leer los registros (FC03) para verificarla.
- `tarea_consola` (prioridad 3): es la aplicación del maestro. Lee comandos por UART0 (`leer`, `escribir`, `escribirm`, `led`, `sondeo`, `tramas`) y los envía a `cola_pedidos`.

## 4. Resultados de las pruebas (todas hechas en el banco)
| Prueba | Resultado |
|---|---|
| 1. Lectura simple | `leer 1 40003` → 496, y en la siguiente lectura 498: el contador avanza |
| 2. Lectura múltiple | `leer 1 40001 7` → los 7 registros correctos |
| 3. Escritura FC06 | `led 1 on` → aceptada y verificada por relectura |
| 4. Control remoto | El LED1 se encendió con FC06. El LED2 parpadeó con período de 300 ms después de un FC16 (Setpoint 1650, Tiempo 300, Modo 2) |
| ADC | Con el pote a mitad de recorrido: 1376 / 1362 / 1387 cuentas, que son 1258 / 1246 / 1267 mV (ruido de ±12 cuentas) |
| 5. Múltiples esclavos | Pendiente del segundo esclavo. El sondeo ya alterna entre las direcciones 1 y 2 |
| 6. Error de comunicación | Con el esclavo desconectado: `Esclavo 1 FUERA DE LINEA`, `TIMEOUT` cada 1 s, y el maestro siguió atendiendo comandos y el sondeo |
| 7. PC | Lectura FC03, escritura FC06 y relectura desde la PC, todas con CRC OK. Un pedido al esclavo 5 (inexistente) no tuvo respuesta |

## 5. Tramas reales capturadas
- **FC03, leer 40001–40004** (PC → esclavo 1): `01 03 00 00 00 04 44 09`
  - Respuesta: `01 03 08 07 D8 06 EA 13 C9 00 00 41 37`
  - Desglose: dirección 01, función 03, 08 bytes de datos, 40001 = 2008, 40002 = 1770 mV, 40003 = 5065, 40004 = 0, CRC = 0x3741 (se transmite 41 37: primero el byte bajo).
- **FC06, 40004 = 0:** `01 06 00 03 00 00 79 CA`. La respuesta es el eco idéntico del pedido.
- **FC03, releer 40004:** `01 03 00 03 00 01 74 0A` → respuesta `01 03 02 00 00 B8 44`
- **FC16, 40005..40007 = 1650, 300, 2:** `01 10 00 04 00 03 06 06 72 01 2C 00 02 DE CC` → respuesta `01 10 00 04 00 03 C1 C9`
- **FC03, leer 40001–40007:** `01 03 00 00 00 07 04 08` → respuesta `01 03 0E 00 00 00 8E 02 07 00 01 06 72 01 2C 00 02 9C 9C`
- **Pedido al esclavo 5:** `05 03 00 00 00 01 85 8E` → sin respuesta. El esclavo 1 lo ignora porque no es su dirección, y el maestro lo informa como TIMEOUT.
- **Orden de bytes:** direcciones, cantidades y valores van en big-endian; el CRC va en little-endian.
- **Separación entre tramas:** silencio de 3,5 caracteres = 3,5 × 11 bits / 9600 ≈ 4 ms. Dentro de una trama no puede haber pausas de más de 1,5 caracteres (≈ 1,7 ms).
- **CRC-16/MODBUS:** polinomio 0xA001 (reflejado), valor inicial 0xFFFF. Si la trama llega con el CRC mal, el receptor la descarta sin responder, y el maestro termina viendo un TIMEOUT.

## 6. Problemas encontrados en la puesta en marcha (sirven para las conclusiones)
1. **TX/RX cruzados en el esquemático:** RO estaba conectado a TX2 y DI a RX2. Síntoma: el maestro transmitía, pero en el bus no aparecía ningún byte (lo verificamos con el adaptador). Se corrigió en las dos placas.
2. **Potenciómetro:** en el esquemático estaba en D21, que no tiene ADC, y alimentado con 5 V, que supera el máximo del pin. Se movió a GPIO34 con alimentación de 3V3.
3. **Terminación:** el módulo MAX485 traía una resistencia de 2,2 kΩ en lugar de 120 Ω. Entre A y B se midieron 1900 Ω. Con cables cortos a 9600 bps funciona igual. Solo deben terminarse los dos extremos del bus.
4. **Adaptador USB-RS485 CH340 clon con driver WCH 3.9 (2024):** acepta una sola configuración del puerto por cada vez que se enchufa. RealTerm falla con "Uno de los dispositivos conectados al sistema no funciona". Se resolvió con el script propio `pc/modbus_pc.py`, que abre el puerto una sola vez.
5. **Dos maestros en el bus:** durante la prueba 7, el ESP32 maestro debe estar en silencio (`sondeo off`). El maestro registró como "drop data received outside of transaction" las tramas que mandaba la PC.

## 7. Pendiente para el informe
- Responder las 15 preguntas de la sección 22 de la consigna.
- Diagrama de conexión y diagrama de arquitectura Maestro/Esclavos.
- Corregir el esquemático (TX/RX y potenciómetro).
- Prueba 5, cuando esté el segundo esclavo.
