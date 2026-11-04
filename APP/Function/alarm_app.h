// 头文件
#ifndef FUNCTION_ALARM_APP_H
#define FUNCTION_ALARM_APP_H

#include <stdint.h>
#include <stdbool.h>

// 宏定义
#define ALARM_MAX_RECORDS    10
#define ALARM_RECORD_SIZE    64
#define ALARM_FLASH_ADDR     0x08010840UL
#define ALARM_FLASH_END      0x08010960UL

// 单条告警记录结构体
typedef struct
{
    uint32_t timestamp;       // UTC seconds
    uint8_t  channel;         // 0=CH0, 1=CH1, 2=CH2
    float    threshold;
    float    actual_value;
    uint8_t  reserved[8];
    uint32_t crc32;
} alarm_record_t;

// 函数声明
void alarm_app_init(void);
bool alarm_app_add_record(uint32_t timestamp, uint8_t channel, float threshold, float actual);
uint8_t alarm_app_get_count(void);
bool alarm_app_get_record(uint8_t index, alarm_record_t *out);
void alarm_app_clear_all(void);
void alarm_app_reset_rising_edge(void);
void alarm_app_reenable_rising_edge(void);
void alarm_app_check_and_trigger(uint32_t timestamp, float ch0_val, float ch1_val, float ch2_val,
                                  float ch0_thresh, float ch1_thresh, float ch2_thresh,
                                  bool active_mode);
bool alarm_app_flush_if_dirty(void);
void alarm_flush_task(void);

// 将 UTC 秒数转为 "YYYY-MM-DD HH:MM:SS" 格式字符串
void timestamp_to_str(uint32_t ts, char *buf);

#endif
