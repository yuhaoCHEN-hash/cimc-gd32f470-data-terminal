// 头文件
#include "alarm_app.h"
#include "usart_app.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>

// 宏定义
#define ALARM_MAGIC            0xA1A10001UL
#define ALARM_STORAGE_SIZE     (ALARM_MAX_RECORDS * sizeof(alarm_record_t))
#define ALARM_HEADER_SIZE      8UL    // magic(4) + count(4)

// 变量定义
static alarm_record_t s_alarm_ram[ALARM_MAX_RECORDS];
static uint32_t s_alarm_count = 0U;
static bool     s_alarm_dirty = false;
static bool     s_initialized = false;

// 上升沿检测：true = 通道当前未越限，可以检测下一次上升沿
static bool     s_rising_ok[3] = {true, true, true};

// 主动上报告警文本缓存
#define ALARM_PENDING_TEXT_MAX  128U
static char     s_pending_alarm_text[ALARM_PENDING_TEXT_MAX];
static bool     s_pending_alarm = false;

// 函数定义

//
// Comment  :       CRC32 计算（IEEE 802.3）
// Parameter:       data - 数据指针
//                  len  - 数据长度
// Return   :       CRC32 校验值
//
static uint32_t alarm_crc32_calc(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint32_t j = 0; j < 8; j++)
        {
            if (crc & 1UL) crc = (crc >> 1UL) ^ 0xEDB88320UL;
            else crc >>= 1UL;
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

//
// Comment  :       计算告警记录 CRC（排除 crc32 字段）
// Parameter:       r - 告警记录指针
// Return   :       CRC32 校验值
//
static uint32_t alarm_record_crc(const alarm_record_t *r)
{
    return alarm_crc32_calc((const uint8_t *)r, offsetof(alarm_record_t, crc32));
}

//
// Comment  :       初始化告警模块：从 Flash 加载记录到 RAM
// Parameter:       none
// Return   :       none
//
void alarm_app_init(void)
{
    const uint32_t *header = (const uint32_t *)ALARM_FLASH_ADDR;
    uint32_t magic = header[0];
    uint32_t count = header[1];

    s_alarm_count = 0U;
    memset(s_alarm_ram, 0, sizeof(s_alarm_ram));

    if (magic != ALARM_MAGIC || count > ALARM_MAX_RECORDS) return;

    const alarm_record_t *flash_recs = (const alarm_record_t *)(ALARM_FLASH_ADDR + ALARM_HEADER_SIZE);
    for (uint32_t i = 0; i < count; i++)
    {
        if (alarm_record_crc(&flash_recs[i]) != flash_recs[i].crc32) break;
        s_alarm_ram[i] = flash_recs[i];
        s_alarm_count++;
    }
}

//
// Comment  :       添加告警记录（最新在前）
// Parameter:       timestamp - UTC 时间戳
//                  channel   - 通道号（0/1/2）
//                  threshold - 阈值
//                  actual    - 实际值
// Return   :       true - 成功
//
bool alarm_app_add_record(uint32_t timestamp, uint8_t channel, float threshold, float actual)
{
    // Shift records to make room at [0] (newest first)
    if (s_alarm_count < ALARM_MAX_RECORDS)
    {
        s_alarm_count++;
    }
    for (int32_t i = (int32_t)s_alarm_count - 1; i > 0; i--)
    {
        s_alarm_ram[i] = s_alarm_ram[i - 1];
    }

    memset(&s_alarm_ram[0], 0, sizeof(alarm_record_t));
    s_alarm_ram[0].timestamp = timestamp;
    s_alarm_ram[0].channel = channel;
    s_alarm_ram[0].threshold = threshold;
    s_alarm_ram[0].actual_value = actual;

    s_alarm_dirty = true;
    return true;
}

//
// Comment  :       获取告警记录数量
// Parameter:       none
// Return   :       告警记录数
//
uint8_t alarm_app_get_count(void)
{
    return (uint8_t)s_alarm_count;
}

//
// Comment  :       获取指定索引的告警记录
// Parameter:       index - 索引
//                  out   - 输出缓冲区
// Return   :       true - 成功，false - 索引无效
//
bool alarm_app_get_record(uint8_t index, alarm_record_t *out)
{
    if (index >= s_alarm_count || out == NULL) return false;
    *out = s_alarm_ram[index];
    return true;
}

//
// Comment  :       清除所有告警记录
// Parameter:       none
// Return   :       none
//
void alarm_app_clear_all(void)
{
    s_alarm_count = 0U;
    memset(s_alarm_ram, 0, sizeof(s_alarm_ram));
    s_alarm_dirty = true;
}

//
// Comment  :       清除告警后重置上升沿检测状态
// Parameter:       none
// Return   :       none
//
void alarm_app_reset_rising_edge(void)
{
    s_rising_ok[0] = false;
    s_rising_ok[1] = false;
    s_rising_ok[2] = false;
}

//
// Comment  :       告警模式变更后重置上升沿检测
// Parameter:       none
// Return   :       none
//
void alarm_app_reenable_rising_edge(void)
{
    s_rising_ok[0] = true;
    s_rising_ok[1] = true;
    s_rising_ok[2] = true;
}

//
// Comment  :       延迟写 Flash：当前实现仅清脏标记
// Parameter:       none
// Return   :       true
//
bool alarm_app_flush_if_dirty(void)
{
    s_alarm_dirty = false;
    return true;
}

extern void rs485_send_string(const char *str);

//
// Comment  :       告警文本发送任务（200ms 周期）
// Parameter:       none
// Return   :       none
//
void alarm_flush_task(void)
{
    if (s_pending_alarm)
    {
        s_pending_alarm = false;
        rs485_send_string(s_pending_alarm_text);
        s_pending_alarm_text[0] = '\0';
    }
}

//
// Comment  :       UTC 时间戳转日期时间字符串
// Parameter:       ts  - UTC 秒数
//                  buf - 输出缓冲区（至少 20 字节）
// Return   :       none
//
void timestamp_to_str(uint32_t ts, char *buf)
{
    uint32_t sec  = ts % 60U; ts /= 60U;
    uint32_t min  = ts % 60U; ts /= 60U;
    uint32_t hour = ts % 24U;
    uint32_t days = ts / 24U;

    uint32_t year = 1970U;
    while (1)
    {
        uint32_t yd = (((year % 4U == 0U) && (year % 100U != 0U)) || (year % 400U == 0U)) ? 366U : 365U;
        if (days < yd) break;
        days -= yd;
        year++;
    }

    uint8_t dim[12] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
    if (((year % 4U == 0U) && (year % 100U != 0U)) || (year % 400U == 0U)) dim[1] = 29U;
    uint32_t month = 1U;
    while (days >= (uint32_t)dim[month - 1U])
    {
        days -= dim[month - 1U];
        month++;
    }
    uint32_t day = days + 1U;

    snprintf(buf, 20, "%04u-%02u-%02u %02u:%02u:%02u",
             year, month, day, hour, min, sec);
}

//
// Comment  :       告警检测与触发：上升沿检测多通道越限
// Parameter:       timestamp  - UTC 时间戳
//                  ch0_val   - CH0 当前值
//                  ch1_val   - CH1 当前值
//                  ch2_val   - CH2 当前值
//                  ch0_thresh - CH0 阈值
//                  ch1_thresh - CH1 阈值
//                  ch2_thresh - CH2 阈值
//                  active_mode - true=主动上报，false=仅存储
// Return   :       none
//
void alarm_app_check_and_trigger(uint32_t timestamp, float ch0_val, float ch1_val, float ch2_val,
                                  float ch0_thresh, float ch1_thresh, float ch2_thresh,
                                  bool active_mode)
{
    struct
    {
        float val;
        float thresh;
        uint8_t ch;
        bool *last_ok;
    } checks[] =
    {
        {ch0_val, ch0_thresh, 0, &s_rising_ok[0]},
        {ch1_val, ch1_thresh, 1, &s_rising_ok[1]},
        {ch2_val, ch2_thresh, 2, &s_rising_ok[2]},
    };

    if (!s_initialized)
    {
        s_initialized = true;
        for (int i = 0; i < 3; i++)
        {
            bool exceeded = (checks[i].thresh > 0.0f && checks[i].val > checks[i].thresh);
            *checks[i].last_ok = !exceeded;
        }
        return;
    }

    for (int i = 0; i < 3; i++)
    {
        if (checks[i].thresh == 0.0f || checks[i].thresh == -0.0f)
        {
            *checks[i].last_ok = true;
            continue;
        }

        bool exceeded = (checks[i].val > checks[i].thresh);

        if (exceeded && *checks[i].last_ok)
        {
            *checks[i].last_ok = false;
            alarm_app_add_record(timestamp, checks[i].ch, checks[i].thresh, checks[i].val);

            if (active_mode)
            {
                char ts_str[20];
                char alarm_str[80];
                timestamp_to_str(timestamp, ts_str);
                snprintf(alarm_str, sizeof(alarm_str),
                         "%s | CH%u | %.2f | %.2f\r\n",
                         ts_str, checks[i].ch, checks[i].thresh, checks[i].val);
                strncpy(s_pending_alarm_text, alarm_str, ALARM_PENDING_TEXT_MAX - 1U);
                s_pending_alarm_text[ALARM_PENDING_TEXT_MAX - 1U] = '\0';
                s_pending_alarm = true;
            }
        }
        else if (!exceeded)
        {
            *checks[i].last_ok = true;
        }
    }
}
