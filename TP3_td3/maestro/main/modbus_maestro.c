// Capa Modbus del maestro. La inicializacion es la del ejemplo que ya se probo
// en el banco (modbus_maestro.c); aca se agregan FC06, FC16 y la validacion.

#include "modbus_maestro.h"

#include <string.h>

#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "mbcontroller.h"

#include "app_config.h"

static const char *TAG = "MB_MAESTRO";

static void *master_handle = NULL;

esp_err_t modbus_maestro_init(void)
{
    // 1. Parametros de comunicacion. El maestro no tiene direccion propia.
    mb_communication_info_t config = {
        .ser_opts.port             = MB_UART_PUERTO,
        .ser_opts.mode             = MB_RTU,
        .ser_opts.baudrate         = MB_BAUDIOS,
        .ser_opts.parity           = MB_PARIDAD,
        .ser_opts.uid              = 0,                         // no se usa en el maestro
        .ser_opts.response_tout_ms = MB_TIMEOUT_RESPUESTA_MS,   // timeout de respuesta
        .ser_opts.data_bits        = UART_DATA_8_BITS,
        .ser_opts.stop_bits        = UART_STOP_BITS_1,
    };

    esp_err_t err = mbc_master_create_serial(&config, &master_handle);
    if (err != ESP_OK || master_handle == NULL) {
        ESP_LOGE(TAG, "Fallo al crear el controlador Modbus maestro (%s)", esp_err_to_name(err));
        return (err != ESP_OK) ? err : ESP_FAIL;
    }

    // 2. Pines fisicos. El 4to parametro es RTS: en modo RS485 half-duplex la
    //    UART lo pone en 1 mientras transmite y en 0 al terminar el ultimo bit.
    //    Va a DE y /RE unidos, asi el MAX485 queda en recepcion el resto del tiempo.
    ESP_RETURN_ON_ERROR(uart_set_pin(MB_UART_PUERTO, MB_PIN_TX, MB_PIN_RX,
                                     MB_PIN_DE_RE, UART_PIN_NO_CHANGE),
                        TAG, "uart_set_pin fallo");
    ESP_RETURN_ON_ERROR(uart_set_mode(MB_UART_PUERTO, UART_MODE_RS485_HALF_DUPLEX),
                        TAG, "uart_set_mode fallo");

    // 3. Arrancar.
    ESP_RETURN_ON_ERROR(mbc_master_start(master_handle), TAG, "mbc_master_start fallo");

    ESP_LOGI(TAG, "Maestro Modbus RTU listo: UART%d, %d 8E1, timeout %d ms",
             MB_UART_PUERTO, MB_BAUDIOS, MB_TIMEOUT_RESPUESTA_MS);
    return ESP_OK;
}

// Controles previos a poner algo en el bus. Una direccion fuera de 1..247 no se
// puede ni transmitir; una direccion valida pero que nadie tiene se detecta
// despues, por timeout.
static esp_err_t validar(uint8_t esclavo, uint16_t cantidad)
{
    if (master_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (esclavo < MB_DIR_MIN || esclavo > MB_DIR_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    if (cantidad == 0 || cantidad > MB_MAX_REG_POR_PEDIDO) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

// Arma la trama, la transmite y espera la respuesta. Bloquea hasta la
// respuesta o hasta el timeout. El stack verifica direccion, funcion y CRC.
static esp_err_t enviar(uint8_t esclavo, uint8_t funcion, uint16_t offset,
                        uint16_t cantidad, void *datos)
{
    esp_err_t err = validar(esclavo, cantidad);
    if (err != ESP_OK) {
        return err;
    }

    mb_param_request_t pedido = {
        .slave_addr = esclavo,
        .command    = funcion,
        .reg_start  = offset,
        .reg_size   = cantidad,
    };
    return mbc_master_send_request(master_handle, &pedido, datos);
}

esp_err_t modbus_maestro_leer(uint8_t esclavo, uint16_t offset, uint16_t cantidad,
                              uint16_t *destino)
{
    return enviar(esclavo, MB_FC_LEER_HOLDING, offset, cantidad, destino);
}

esp_err_t modbus_maestro_escribir_uno(uint8_t esclavo, uint16_t offset, uint16_t valor)
{
    return enviar(esclavo, MB_FC_ESCRIBIR_UNO, offset, 1, &valor);
}

esp_err_t modbus_maestro_escribir_varios(uint8_t esclavo, uint16_t offset, uint16_t cantidad,
                                         const uint16_t *valores)
{
    // El stack pide un puntero no-const; se copia para no romper la promesa.
    uint16_t copia[MB_MAX_REG_POR_PEDIDO];
    if (cantidad > MB_MAX_REG_POR_PEDIDO) {
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(copia, valores, cantidad * sizeof(uint16_t));
    return enviar(esclavo, MB_FC_ESCRIBIR_VARIOS, offset, cantidad, copia);
}

const char *modbus_maestro_describir_error(esp_err_t err)
{
    switch (err) {
        case ESP_OK:
            return "OK";
        case ESP_ERR_TIMEOUT:
            // Tambien cae aca una respuesta con CRC incorrecto: el stack la
            // descarta como si no hubiera llegado nada.
            return "TIMEOUT: sin respuesta (esclavo desconectado, direccion inexistente o trama con CRC invalido descartada)";
        case ESP_ERR_INVALID_RESPONSE:
            return "RESPUESTA INVALIDA: trama con formato, largo o funcion incorrectos";
        case ESP_ERR_NOT_SUPPORTED:
            return "EXCEPCION: el esclavo no soporta la funcion pedida";
        case ESP_FAIL:
            return "EXCEPCION Modbus del esclavo (p. ej. direccion de registro ilegal)";
        case ESP_ERR_INVALID_ARG:
            return "DIRECCION INVALIDA: el esclavo debe estar entre 1 y 247";
        case ESP_ERR_INVALID_SIZE:
            return "CANTIDAD INVALIDA de registros";
        case ESP_ERR_INVALID_STATE:
            return "ERROR DE COMUNICACION: stack Modbus no inicializado u ocupado";
        default:
            return "ERROR DE COMUNICACION";
    }
}
