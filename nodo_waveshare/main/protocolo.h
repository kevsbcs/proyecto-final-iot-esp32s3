/**
 * @file protocolo.h
 * @brief Trama compacta para enlace ESP-NOW entre los dos nodos.
 * @warning Trama sin cifrado ni autenticacion: usar solo en laboratorio.
 */
#ifndef IOT_PROTOCOLO_H
#define IOT_PROTOCOLO_H

#include <stdint.h>

#define IOT_MAGIC 0x49544F49UL /* "IOTI" little endian */
#define IOT_PROTO_VER 1

typedef enum {
    IOT_T_HELLO = 1,
    IOT_T_COMANDO = 2,
    IOT_T_ACK = 3
} iot_tipo_t;

typedef enum {
    IOT_CMD_NINGUNO = 0,
    IOT_CMD_ON = 1,
    IOT_CMD_OFF = 2,
    IOT_CMD_ESTADO = 3
} iot_comando_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t version;
    uint8_t tipo;
    uint8_t comando;
    uint8_t estado_led;
    uint32_t secuencia;
} iot_trama_t;

_Static_assert(sizeof(iot_trama_t) == 12, "Trama ESP-NOW debe ser de 12 bytes");

/** @brief Construye una trama de protocolo version 1. */
static inline iot_trama_t iot_trama(uint8_t tipo, uint8_t cmd, uint8_t led, uint32_t seq)
{
    iot_trama_t t = { IOT_MAGIC, IOT_PROTO_VER, tipo, cmd, led, seq };
    return t;
}

/** @brief Valida longitud, firma magica y version del paquete. */
static inline int iot_trama_valida(const iot_trama_t *t)
{
    return t && t->magic == IOT_MAGIC && t->version == IOT_PROTO_VER;
}
#endif