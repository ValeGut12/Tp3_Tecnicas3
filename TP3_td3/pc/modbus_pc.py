"""
Herramienta de PC para el TP3: maestro Modbus RTU y monitor del bus RS485,
a traves del adaptador USB-RS485.

Abre el puerto UNA sola vez y hace todo dentro de esa sesion. Es a proposito:
el driver CH340 v3.9 (2024) con adaptadores clon acepta una sola
configuracion por cada vez que se enchufa, y RealTerm/QModMaster fallan.

Uso (con el Python de ESP-IDF, que ya trae pyserial):
    python modbus_pc.py COM3

Comandos:
    leer <esc> <reg> [cant]        FC03   ej: leer 1 40001 4
    escribir <esc> <reg> <valor>   FC06   ej: escribir 1 40004 1
    escribirm <esc> <reg> <v1> ... FC16   ej: escribirm 1 40005 1650 300 2
    monitor [segundos]             escucha el bus y separa las tramas (Ctrl+C corta)
    salir

IMPORTANTE: mientras la PC hace de maestro, el ESP32 maestro tiene que estar
callado ("sondeo off"), si no las tramas chocan en el bus.
"""

import sys
import time

import serial

BAUDIOS = 9600
TIMEOUT_RESPUESTA_S = 1.0
# Silencio de 3,5 caracteres que separa tramas RTU: 3,5 * 11 bits / 9600 = 4 ms.
# Se usa un margen mayor porque el USB entrega los bytes en paquetes.
SILENCIO_FIN_TRAMA_S = 0.015

NOMBRES = {
    40001: "Analogico 1 (ADC crudo)",
    40002: "Analogico 2 (mV)",
    40003: "Contador",
    40004: "Estado (LED1)",
    40005: "Setpoint (mV)",
    40006: "Tiempo (ms)",
    40007: "Modo (LED2)",
}


def crc16(datos: bytes) -> int:
    crc = 0xFFFF
    for b in datos:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def con_crc(datos: bytes) -> bytes:
    c = crc16(datos)
    return datos + bytes([c & 0xFF, c >> 8])  # CRC: byte bajo primero


def crc_ok(trama: bytes) -> bool:
    return len(trama) >= 4 and crc16(trama[:-2]) == (trama[-2] | trama[-1] << 8)


def hexa(b: bytes) -> str:
    return " ".join(f"{x:02X}" for x in b)


def leer_trama(puerto, timeout_s):
    """Lee una trama completa: espera el primer byte y corta con el silencio."""
    limite = time.time() + timeout_s
    trama = b""
    ultimo = None
    while True:
        dato = puerto.read(256)
        ahora = time.time()
        if dato:
            trama += dato
            ultimo = ahora
        elif trama and ahora - ultimo >= SILENCIO_FIN_TRAMA_S:
            return trama
        elif not trama and ahora >= limite:
            return b""


def transaccion(puerto, pedido: bytes):
    puerto.reset_input_buffer()
    puerto.write(pedido)
    puerto.flush()
    print(f"   TX: {hexa(pedido)}")
    resp = leer_trama(puerto, TIMEOUT_RESPUESTA_S)
    if not resp:
        print("   RX: (nada) -> TIMEOUT: el esclavo no respondio")
        return None
    print(f"   RX: {hexa(resp)}   CRC {'OK' if crc_ok(resp) else 'INCORRECTO'}")
    if not crc_ok(resp):
        return None
    if resp[1] & 0x80:
        print(f"   EXCEPCION Modbus: codigo {resp[2]}")
        return None
    return resp


def registro(txt: str) -> int:
    reg = int(txt, 0)
    if not 40001 <= reg <= 49999:
        raise ValueError("usar numeracion 4xxxx, ej. 40001")
    return reg


def cmd_leer(puerto, esc, reg, cant=1):
    off = reg - 40001
    resp = transaccion(puerto, con_crc(bytes([esc, 0x03, off >> 8, off & 0xFF, 0, cant])))
    if resp:
        for i in range(cant):
            v = resp[3 + 2 * i] << 8 | resp[4 + 2 * i]
            print(f"   {reg + i}  {NOMBRES.get(reg + i, ''):24s} = {v:5d}  (0x{v:04X})")


def cmd_escribir(puerto, esc, reg, valor):
    off = reg - 40001
    if transaccion(puerto, con_crc(bytes([esc, 0x06, off >> 8, off & 0xFF, valor >> 8, valor & 0xFF]))):
        print("   Escritura aceptada")


def cmd_escribirm(puerto, esc, reg, valores):
    off = reg - 40001
    n = len(valores)
    datos = bytes([esc, 0x10, off >> 8, off & 0xFF, 0, n, 2 * n])
    for v in valores:
        datos += bytes([v >> 8, v & 0xFF])
    if transaccion(puerto, con_crc(datos)):
        print("   Escritura multiple aceptada")


def cmd_monitor(puerto, segundos=None):
    print("   Escuchando el bus (Ctrl+C para cortar)...")
    fin = time.time() + segundos if segundos else None
    t0 = time.time()
    try:
        while fin is None or time.time() < fin:
            trama = leer_trama(puerto, 0.5)
            if trama:
                fc = trama[1] if len(trama) > 1 else 0
                print(f"   [{time.time() - t0:8.3f} s] dir={trama[0]:3d} fc={fc:02d}  "
                      f"{hexa(trama)}   CRC {'OK' if crc_ok(trama) else 'INCORRECTO'}")
    except KeyboardInterrupt:
        print()


def main():
    if len(sys.argv) < 2:
        print("Uso: python modbus_pc.py COMx")
        sys.exit(1)

    try:
        puerto = serial.Serial(sys.argv[1], BAUDIOS, parity=serial.PARITY_EVEN,
                               bytesize=8, stopbits=1, timeout=0.002)
    except serial.SerialException as e:
        print(f"No se pudo abrir {sys.argv[1]}: {e}")
        print("Con el CH340: desenchufar y volver a enchufar el adaptador, y reintentar.")
        sys.exit(1)

    print(f"{sys.argv[1]} abierto a {BAUDIOS} 8E1. No cierres el programa hasta terminar.")
    print(__doc__.split("Comandos:")[1].split("IMPORTANTE")[0])

    while True:
        try:
            linea = input("pc> ").split()
        except (EOFError, KeyboardInterrupt):
            break
        if not linea:
            continue
        cmd, args = linea[0].lower(), linea[1:]
        try:
            if cmd in ("salir", "exit", "q"):
                break
            elif cmd in ("leer", "r"):
                cmd_leer(puerto, int(args[0]), registro(args[1]), int(args[2]) if len(args) > 2 else 1)
            elif cmd in ("escribir", "w"):
                cmd_escribir(puerto, int(args[0]), registro(args[1]), int(args[2], 0))
            elif cmd in ("escribirm", "wm"):
                cmd_escribirm(puerto, int(args[0]), registro(args[1]), [int(v, 0) for v in args[2:]])
            elif cmd == "monitor":
                cmd_monitor(puerto, float(args[0]) if args else None)
            else:
                print("   Comando desconocido")
        except (IndexError, ValueError) as e:
            print(f"   Argumentos invalidos ({e})")

    puerto.close()


if __name__ == "__main__":
    main()
