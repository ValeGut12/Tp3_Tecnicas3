# esp-modbus se agrega solo a REQUIRES porque esta declarado en idf_component.yml.
idf_component_register(
    SRCS
        "main.c"
        "registros.c"
        "modbus_esclavo.c"
        "tarea_modbus.c"
        "tarea_adquisicion.c"
        "tarea_aplicacion.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        esp_driver_uart
        esp_driver_gpio
        esp_adc
)
