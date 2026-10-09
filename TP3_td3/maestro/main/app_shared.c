#include "app_shared.h"

QueueHandle_t cola_pedidos = NULL;

bool app_shared_crear_objetos(void)
{
    cola_pedidos = xQueueCreate(COLA_PEDIDOS_LARGO, sizeof(pedido_t));
    return (cola_pedidos != NULL);
}
