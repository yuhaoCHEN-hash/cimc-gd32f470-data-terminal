// 头文件
#ifndef PROTOCOL_CRC16_MODBUS_H
#define PROTOCOL_CRC16_MODBUS_H

#include <stdint.h>

// 函数声明
uint16_t crc16_modbus(const uint8_t *data, uint32_t len);

#endif
