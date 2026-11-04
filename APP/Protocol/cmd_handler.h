// 头文件
#ifndef PROTOCOL_CMD_HANDLER_H
#define PROTOCOL_CMD_HANDLER_H

#include <stdint.h>
#include <stdbool.h>
#include "frame_parser.h"

// Baud rate mapping: 0x11=4800, 0x12=9600, 0x13=19200, 0x14=115200
#define BAUD_MAP_4800   0x11
#define BAUD_MAP_9600   0x12
#define BAUD_MAP_19200  0x13
#define BAUD_MAP_115200 0x14

// Default firmware version: 2.0.1.0
#define FW_VER_MAJOR    2
#define FW_VER_MINOR    0
#define FW_VER_PATCH    1
#define FW_VER_BUILD    0

// Alarm mode
#define ALARM_ACTIVE    0x01
#define ALARM_PASSIVE   0x02

// Auto report interval mapping: 01=1s, 02=3s, 03=5s
#define REPORT_INTV_1S  0x01
#define REPORT_INTV_3S  0x02
#define REPORT_INTV_5S  0x03

// 变量声明
extern volatile bool g_auto_report_active;

// 函数声明
void cmd_handler_init(uint16_t device_id);

// Process one parsed request frame, write ASCII response to out_ascii.
//    Returns ASCII length (0 = no response).
uint32_t cmd_handler_process(const protocol_frame_t *req, char *out_ascii);

// Called by main loop / scheduler periodically
void cmd_handler_poll(void);

// Getters for Function layer
uint16_t cmd_handler_get_device_id(void);
uint8_t  cmd_handler_get_baud_rate_code(void);
uint32_t cmd_handler_get_baud_rate_value(void);
bool     cmd_handler_is_auto_reporting(void);
uint8_t  cmd_handler_get_report_interval(void);
bool     cmd_handler_is_sleeping(void);

// Set by Function layer when sleep wakeup occurs
void cmd_handler_notify_wakeup(void);

#endif
