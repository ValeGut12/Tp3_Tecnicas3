// Inicializacion del stack Modbus esclavo. Basado en el ejemplo que ya se probo
// en el banco (modbus_esclavo1.c).

#include "modbus_esclavo.h"

#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "mbcontroller.h"

#include "app_config.h"
#include "registros.h"

_Static_assert(MB_DIRECCION_ESCLAVO >= 1 && MB_DIRECCION_ESCLAVO <= 247,
               "MB_DIRECCION_ESCLAVO tiene que estar entre 1 y 247");

static const char *TAG = "MB_ESCLAVO";

static void *slave_handle = NULL;

esp_err_t modbus_esclavo_init(void)
{
    // 1. Parametros de comunicacion. uid es la direccion propia: el stack
    //    descarta en silencio toda trama dirigida a otra direccion.
    mb_communication_info_t config = {
        .ser_opts.port      = MB_UART_PUERTO,
        .ser_opts.mode      = MB_RTU,
        .ser_opts.baudrate  = MB_BAUDIOS,
        .ser_opts.parity    = MB_PARIDAD,
        .ser_opts.uid       = MB_DIRECCION_ESCLAVO,
        .ser_opts.data_bits = UART_DATA_8_BITS,
        .ser_opts.stop_bits = UART_STOP_BITS_1,
    };

    esp_err_t err = mbc_slave_create_serial(&config, &slave_handle);
    if (err != ESP_OK || slave_handle == NULL) {
        ESP_LOGE(TAG, "Fallo al crear el controlador Modbus esclavo (%s)", esp_err_to_name(err));
        return (err != ESP_OK) ? err : ESP_FAIL;
    }

    // 2. Pines fisicos. El 4to parametro es RTS: en modo RS485 half-duplex la
    //    UART lo pone en 1 mientras transmite y en 0 al terminar el ultimo bit,
    //    que es exactamente lo que necesitan DE y /RE del MAX485.
    ESP_RETURN_ON_ERROR(uart_set_pin(MB_UART_PUERTO, MB_PIN_TX, MB_PIN_RX,
                                     MB_PIN_DE_RE, UART_PIN_NO_CHANGE),
                        TAG, "uart_set_pin fallo");
    ESP_RETURN_ON_ERROR(uart_set_mode(MB_UART_PUERTO, UART_MODE_RS485_HALF_DUPLEX),
                        TAG, "uart_set_mode fallo");

    // 3. "Esta zona de memoria son los Holding Registers 40001 en adelante".
    mb_register_area_descriptor_t area = {
        .type         = MB_PARAM_HOLDING,
        .start_offset = 0,                          // 40001 = offset 0
        .address      = (void *)registros_area(),
        .size         = registros_area_bytes(),     // en bytes
        .access       = MB_ACCESS_RW,
    };
    ESP_RETURN_ON_ERROR(mbc_slave_set_descriptor(slave_handle, area),
                        TAG, "no se pudo registrar el mapa de registros");

    // A partir de aca las tareas acceden al mapa con el lock del controlador.
    registros_asignar_controlador(slave_handle);

    // 4. Arrancar. Desde este momento el esclavo responde al maestro.
    ESP_RETURN_ON_ERROR(mbc_slave_start(slave_handle), TAG, "mbc_slave_start fallo");

    ESP_LOGI(TAG, "Esclavo Modbus RTU listo: direccion %d, UART%d, %d 8E1",
             MB_DIRECCION_ESCLAVO, MB_UART_PUERTO, MB_BAUDIOS);
    return ESP_OK;
}

void *modbus_esclavo_handle(void)
{
    return slave_handle;
}
