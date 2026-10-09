# TP3 – Modbus RTU sobre RS485 con ESP32 (ESP-IDF + FreeRTOS)

Dos proyectos ESP-IDF independientes:

| Carpeta    | Qué es                                   | Se graba en                  |
|------------|------------------------------------------|------------------------------|
| `maestro/` | Maestro Modbus con consola de comandos   | 1 ESP32                      |
| `esclavo/` | Esclavo Modbus (ADC, contador, 2 LEDs)   | cada ESP32 esclavo           |

Dependencia: `espressif/esp-modbus ^2.0.0`. La baja el Component Manager la primera vez que se compila, así que en ese momento hace falta internet.

```
cd esclavo   (o maestro)
idf.py set-target esp32
idf.py build flash monitor
```

**Dirección de cada esclavo:** en `esclavo/main/app_config.h` hay que cambiar `MB_DIRECCION_ESCLAVO` (1, 2, 3…) y grabar cada placa con su dirección.
**Esclavos que sondea el maestro:** se definen en `maestro/main/app_config.h` → `MB_ESCLAVOS_SONDEO { 1, 2 }`. Cuando se sume el tercero, se agrega su dirección ahí.

---

## 1. Conexionado (igual en maestro y esclavos)

| ESP32            | Conversor de nivel | MAX485   | Función |
|------------------|--------------------|----------|---------|
| GPIO17 (TX2)     | LV → HV            | **DI**   | Datos que transmite el ESP32 |
| GPIO16 (RX2)     | HV → LV            | **RO**   | Datos que el MAX485 recibe del bus |
| GPIO32           | (directo o LV→HV)  | **DE + /RE** unidos | 1 = transmitir, 0 = recibir |
| —                | —                  | **A / B**| Par diferencial del bus |
| 5V / GND         | HV / GND           | VCC / GND| Alimentación del MAX485 |

- **DE/RE:** los maneja la UART por hardware. GPIO32 se configura como RTS y la UART queda en modo `UART_MODE_RS485_HALF_DUPLEX`: pone RTS en 1 mientras transmite y lo vuelve a 0 apenas sale el último bit de stop. Así el MAX485 vuelve a recepción a tiempo para escuchar la respuesta.
- **Salidas del esclavo:** LED1 en GPIO22 y LED2 en GPIO23, cada uno con 1 kΩ a GND.
- **Potenciómetro del esclavo:** cursor a **GPIO34** (ADC1_CH6). Los extremos van a **3V3** y GND, **no a 5 V**.
- **Bus:** A con A y B con B en todos los nodos, incluido el adaptador USB-RS485. Van 120 Ω solo en los dos extremos del cable.

---

## 2. Mapa de registros del esclavo (`mapa_modbus.h`)

| Registro | Offset en la trama | Descripción | Quién lo escribe |
|---------|--------------------|-------------|------------------|
| 40001 | 0x0000 | Valor analógico 1: cuentas ADC 0..4095 (GPIO34) | tarea de adquisición |
| 40002 | 0x0001 | Valor analógico 2: tensión del pote en mV (calibrada) | tarea de adquisición |
| 40003 | 0x0002 | Contador, +1 por segundo | tarea de adquisición |
| 40004 | 0x0003 | Estado: 0 = OFF, 1 = ON → LED1 (GPIO22) | maestro (FC06) |
| 40005 | 0x0004 | Setpoint en mV (lo usa el modo 3) | maestro (FC16) |
| 40006 | 0x0005 | Tiempo: período de parpadeo en ms (lo usa el modo 2) | maestro (FC16) |
| 40007 | 0x0006 | Modo de operación de LED2 (GPIO23) | maestro (FC16) |

Modos de 40007:

| Modo | LED2 |
|------|------|
| 0 | apagado |
| 1 | encendido |
| 2 | parpadea con período = 40006 ms |
| 3 | encendido si 40002 > 40005 (el pote supera el setpoint) |

Valores iniciales: Estado 0, Setpoint 1650 mV, Tiempo 500 ms, Modo 0.
Si el maestro escribe 40001..40003, la tarea de adquisición los pisa en el ciclo siguiente.

---

## 3. Tareas FreeRTOS

### Esclavo
| Tarea | Prio | Qué hace | Sincronización |
|-------|------|----------|----------------|
| `tarea_modbus` | 5 | Espera los accesos del maestro con `mbc_slave_check_event()` y los muestra por consola. Si el maestro escribió, despierta a la aplicación. | Notificación directa (`xTaskNotifyGive`) a la aplicación |
| `tarea_aplicacion` | 4 | 40004 → LED1; 40005..40007 → LED2 según el modo | `ulTaskNotifyTake` con timeout de 10 ms: reacciona apenas escribe el maestro y además mantiene el parpadeo |
| `tarea_adquisicion` | 3 | Cada 100 ms lee el ADC (promedio de 8 muestras) y actualiza 40001/40002. Cada 1 s suma 1 a 40003 | `vTaskDelayUntil` para un período fijo |

La recepción, el CRC, el filtrado por dirección y la respuesta los resuelve el stack esp-modbus en su propia tarea. Todos los accesos del programa al mapa pasan por `registros.c`, que toma `mbc_slave_lock()/unlock()`. Así el stack nunca responde un bloque a medio actualizar.

### Maestro
| Tarea | Prio | Qué hace | Sincronización |
|-------|------|----------|----------------|
| `tarea_modbus` | 5 | Es la única que usa el bus. Arma las solicitudes (FC03/06/16), transmite, espera la respuesta, la procesa, detecta errores y timeouts, y hace el sondeo cíclico | Recibe de `cola_pedidos`. El timeout de la cola hace de período del sondeo |
| `tarea_consola` | 3 | Es la aplicación del maestro: lee comandos por USB (UART0), los valida y los manda como pedidos | Envía a `cola_pedidos` |

---

## 4. Comandos de la consola del maestro

```
leer <esc> <reg> [cant]         FC03  ej: leer 1 40001 4
escribir <esc> <reg> <valor>    FC06  ej: escribir 1 40004 1
escribirm <esc> <reg> <v1> ...  FC16  ej: escribirm 1 40005 2000 500 2
led <esc> on|off                FC06 sobre 40004
sondeo on|off [periodo_ms]      lee 40001-40004 alternando los esclavos
tramas on|off                   muestra los bytes de cada trama con su CRC
ayuda
```
Después de cada escritura, el maestro vuelve a leer los registros escritos (FC03) para confirmar que el esclavo tomó los valores.

---

## 5. Pruebas obligatorias (sección 17)

| # | Prueba | Cómo se hace |
|---|--------|--------------|
| 1 | Lectura | `leer 1 40003` |
| 2 | Lectura múltiple | `leer 1 40001 4` |
| 3 | Escritura | `escribir 1 40005 1200` |
| 4 | Control remoto | `led 1 on` / `led 1 off` → cambia LED1. `escribirm 1 40005 1650 300 2` → LED2 parpadea |
| 5 | Múltiples esclavos | `sondeo on` → alterna Esclavo 1 / Esclavo 2 |
| 6 | Error de comunicación | Con `sondeo on`, desconectar A/B de un esclavo: aparece `TIMEOUT` y `FUERA DE LINEA` para ese esclavo, y el otro se sigue leyendo. Al reconectarlo aparece `EN LINEA`. `leer 5 40001` muestra el caso de dirección inexistente |
| 7 | PC | Ver abajo |

### Prueba 7 – PC con el adaptador USB-RS485
**Herramienta recomendada: [pc/modbus_pc.py](pc/modbus_pc.py).** El driver CH340 v3.9 (2024), con los adaptadores clon, acepta **una sola configuración del puerto por cada vez que se enchufa**. RealTerm y QModMaster configuran el puerto en dos pasos y fallan con "Uno de los dispositivos conectados al sistema no funciona". Este script abre el puerto una sola vez y hace todo en esa sesión:
```
C:\Espressif\tools\python\v6.0.2\venv\Scripts\python.exe pc\modbus_pc.py COM3
pc> leer 1 40001 4
pc> escribir 1 40004 1
pc> escribirm 1 40005 1650 300 2
pc> monitor          (escucha el bus y separa las tramas con su CRC; Ctrl+C corta)
```
Si no abre: desenchufar y volver a enchufar el adaptador, y ejecutarlo de nuevo.

Otras herramientas posibles, con un driver CH340 anterior (3.5.2019): **QModMaster**, **Modbus Poll** o **mbpoll** (línea de comandos). Todas se configuran en RTU, 9600, 8, Even, 1.

**Importante:** en un bus Modbus hay un solo maestro por vez. Mientras la PC haga de maestro, el ESP32 maestro tiene que estar callado (`sondeo off` y sin mandar comandos). Si no, las dos tramas chocan en el bus.

Ejemplo con mbpoll (las direcciones son 1-based, así que `-r 1` es 40001):
```
mbpoll -m rtu -b 9600 -P even -a 1 -t 4 -r 1 -c 4 COM5        # leer 40001..40004
mbpoll -m rtu -b 9600 -P even -a 1 -t 4 -r 4 COM5 1           # escribir 40004 = 1
```
Para **observar** el tráfico entre el ESP32 maestro y los esclavos sin intervenir, se abre el puerto COM con un monitor serie en hexadecimal a 9600 8E1 (por ejemplo, el "Bus Monitor" de QModMaster, o RealTerm en modo Hex).

---

## 6. Análisis de trama (secciones 11 y 12)

Con `tramas on`, el maestro muestra los bytes de cada pedido y de su respuesta. Los CRC de estos ejemplos están calculados con `crc16_modbus.c`:

**FC03: leer 40001..40004 del esclavo 1**
```
01  03  00 00  00 04  44 09
│   │   │      │      └─ CRC-16: primero el byte BAJO (0x44), después el ALTO (0x09) → CRC = 0x0944
│   │   │      └─ cantidad de registros = 4
│   │   └─ registro inicial = offset 0 (= 40001), byte alto primero
│   └─ función 03 (Read Holding Registers)
└─ dirección del esclavo
```
**FC06: 40004 = 1 (LED1 ON)** → `01 06 00 03 00 01 B8 0A`
**FC16: 40005..40007 = 2000, 250, 2** → `01 10 00 04 00 03 06 07 D0 00 FA 00 02 C6 C0`

- **Orden de bytes:** direcciones, cantidades y valores van en big-endian (byte alto primero). El CRC es la excepción: va en little-endian.
- **Separación entre tramas:** en RTU una trama termina cuando el bus queda en silencio 3,5 caracteres. A 9600 8E1 cada carácter tiene 11 bits, así que 3,5 × 11 / 9600 ≈ **4 ms**. Dentro de una trama no puede haber pausas de más de 1,5 caracteres (≈ 1,7 ms).
- **CRC incorrecto:** el receptor recalcula el CRC sobre dirección + función + datos. Si no coincide, descarta la trama **sin responder**; en Modbus no existe un "NACK" por CRC. Para el maestro, el resultado es un **TIMEOUT**.

---

## 7. Observaciones sobre el esquemático (revisar antes de entregar el diagrama)

1. **El potenciómetro está en D21**, un pin sin ADC, y además va a **+5 V**. Se pasa a **GPIO34** alimentado desde **3V3**. Hay que sacar el puente de D34/D35 a GND.
2. **TX/RX:** según los nombres de red del dibujo, RO termina en TX2 y DI viene de RX2 (LVTX está en RX2 y LVRX en TX2). Lo correcto es **RO → RX2 (GPIO16)** y **DI ← TX2 (GPIO17)**. El ejemplo de prueba funcionó con TX = 17 y RX = 16, así que el cableado real está bien y lo que conviene corregir es el dibujo.
3. **Terminación:** muchos módulos MAX485 traen de fábrica una resistencia de 120 Ω (suele ser R7) y resistencias de polarización. Solo tienen que quedar las de los **dos extremos** del bus; en los nodos intermedios hay que sacarlas. Si no, con 3 o 4 nodos la carga queda demasiado baja.
