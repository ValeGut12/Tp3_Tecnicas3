# esp-modbus se agrega solo a REQUIRES porque esta declarado en idf_component.yml.
idf_component_register(
    SRCS
        "main.c"
        "app_shared.c"
        "crc16_modbus.c"
        "modbus_maestro.c"
        "tarea_modbus.c"
        "tarea_consola.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        esp_driver_uart
        esp_driver_gpio
)
