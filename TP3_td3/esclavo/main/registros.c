// Mapa de Holding Registers y su proteccion contra accesos concurrentes.

#include "registros.h"

#include "mbcontroller.h"

#include "app_config.h"

// Aca viven los valores reales de 40001..40007. El stack Modbus lee y escribe
// directamente sobre esta memoria (se la pasamos con mbc_slave_set_descriptor).
static uint16_t holding[MB_CANT_REG_HOLDING];

static void *controlador = NULL;

static inline void bloquear(void)
{
    if (controlador != NULL) {
        (void)mbc_slave_lock(controlador);
    }
}

static inline void desbloquear(void)
{
    if (controlador != NULL) {
        (void)mbc_slave_unlock(controlador);
    }
}

void registros_valores_iniciales(void)
{
    holding[MB_REG_ADC_CRUDO] = 0;
    holding[MB_REG_ADC_MV]    = 0;
    holding[MB_REG_CONTADOR]  = 0;
    holding[MB_REG_ESTADO]    = ESTADO_INICIAL;
    holding[MB_REG_SETPOINT]  = SETPOINT_INICIAL_MV;
    holding[MB_REG_TIEMPO]    = TIEMPO_INICIAL_MS;
    holding[MB_REG_MODO]      = MODO_INICIAL;
}

uint16_t *registros_area(void)
{
    return holding;
}

uint32_t registros_area_bytes(void)
{
    return sizeof(holding);
}

void registros_asignar_controlador(void *handle_modbus)
{
    controlador = handle_modbus;
}

uint16_t registros_leer(uint16_t offset)
{
    if (offset >= MB_CANT_REG_HOLDING) {
        return 0;
    }
    bloquear();
    uint16_t valor = holding[offset];
    desbloquear();
    return valor;
}

void registros_escribir(uint16_t offset, uint16_t valor)
{
    if (offset >= MB_CANT_REG_HOLDING) {
        return;
    }
    bloquear();
    holding[offset] = valor;
    desbloquear();
}

void registros_leer_bloque(uint16_t offset, uint16_t cantidad, uint16_t *destino)
{
    if (offset + cantidad > MB_CANT_REG_HOLDING) {
        return;
    }
    bloquear();
    for (uint16_t i = 0; i < cantidad; i++) {
        destino[i] = holding[offset + i];
    }
    desbloquear();
}

void registros_escribir_bloque(uint16_t offset, uint16_t cantidad, const uint16_t *valores)
{
    if (offset + cantidad > MB_CANT_REG_HOLDING) {
        return;
    }
    bloquear();
    for (uint16_t i = 0; i < cantidad; i++) {
        holding[offset + i] = valores[i];
    }
    desbloquear();
}

void registros_incrementar_contador(void)
{
    bloquear();
    holding[MB_REG_CONTADOR]++;     // desborda de 65535 a 0, es lo esperable
    desbloquear();
}
