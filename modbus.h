#ifndef __modbusCRC_H
#define __modbusCRC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct _modbus_handler {
  uint8_t slave;
  uint8_t func;
  uint16_t offset;
  uint8_t size;
  uint8_t replySize;
} MODBUS_HANDLER;

uint16_t modbusCRC(const uint8_t *nData, uint8_t len);
void makeModbusPacket(uint8_t *data, uint8_t len, MODBUS_HANDLER *handler);

#ifdef __cplusplus
}
#endif

#endif /* end of __modbusCRC_H */
