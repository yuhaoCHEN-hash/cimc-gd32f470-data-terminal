// 头文件
#ifndef FUNCTION_PARAM_STORE_H
#define FUNCTION_PARAM_STORE_H

#include <stdint.h>
#include <stdbool.h>

//
// APP 参数存储地址说明（赛题 2.5 节，参数区 0x08010000~0x08010FFF，4KB）
//
// BL 占用布局：
//   0x08010000  bl_param_t 主份  (256B)
//   0x08010100  bl_param_t 备份  (256B)
//   0x08010200  日志环形区       (1024B，32条×32B)
//   0x08010600  ← BL 数据结束
//
// APP 布局（由本模块管理，统一使用 flash_page_partial_update 读改写）：
//   0x08010800  app_param_t      (40B，含 CRC)     ← APP_PARAM_FLASH_ADDR
//   0x08010828  ← app_param_t 结束
//   0x08010840  alarm_header + alarm_records (288B) ← alarm_app.h 中 ALARM_FLASH_ADDR
//   0x08010960  ← 告警区结束（远小于页末 0x08010FFF）
//
// 写操作必须整页读改写，不能只擦只写，否则破坏 BL 的 OTA 状态。
// 所有写操作通过 flash_page_partial_update() 序列化，使用唯一共享缓冲区。
//
#define APP_PARAM_FLASH_ADDR    0x08010800UL
#define APP_PARAM_FLASH_PAGE    0x08010000UL    // 所在页首地址（4KB对齐）
#define APP_PARAM_PAGE_SIZE     0x00001000UL    // 参数区页大小 4KB

// 魔术字，区别于 BL magic (0x5AA5C33C)，防止误判
#define APP_PARAM_MAGIC         0xA55AC33CUL

//
// APP 持久化参数结构体。
// 共 40 字节（10 个 word），按 4 字节对齐，fmc_word_program 逐 word 写入。
//
// 赛题 2.4 节要求保存：CH0/CH1 变比、CH0/CH1 阈值、设备 ID、波特率配置。
// crc32 覆盖前 36 字节（magic ~ utc_base_saved），保证掉电恢复数据完整性。
//
typedef struct
{
    uint32_t magic;          // 有效性标志，= APP_PARAM_MAGIC
    uint16_t device_id;      // 设备 ID，范围 0x0001~0xFFFE
    uint8_t  baud_code;      // 波特率代码：0x11=4800 / 0x12=9600 / 0x13=19200 / 0x14=115200
    uint8_t  alarm_mode;     // 告警模式：01=主动上报 / 02=仅存储
    uint8_t  report_intv;    // 自动上报间隔代码：01=1s / 02=3s / 03=5s
    uint8_t  dac_hi;         // DAC 12-bit 输出高字节 bits[11:8]，掉电恢复 CH1 读值
    uint8_t  dac_lo;         // DAC 12-bit 输出低字节 bits[7:0]
    uint8_t  _pad;           // 对齐填充
    float    ch0_ratio;      // CH0 变比（电位器通道）
    float    ch1_ratio;      // CH1 变比（DAC 回读通道）
    float    ch0_thresh;     // CH0 阈值
    float    ch1_thresh;     // CH1 阈值
    float    ch2_thresh;     // CH2 阈值（PT100，保留）
    uint32_t utc_base_saved; // 上次通过 0x0105 设置的 UTC 秒级时间戳，重启后恢复
    uint32_t crc32;          // CRC32 校验（覆盖 magic ~ utc_base_saved 共 36 字节）
} app_param_t;               // 共 40 字节

//
// 从 Flash 加载 APP 参数到 *out。
//
bool param_load(app_param_t *out);

bool param_save(const app_param_t *p);

bool flash_page_partial_update(uint32_t page_offset, const void *data, uint32_t len);

bool param_set_upgrade_pending(void);

#endif // FUNCTION_PARAM_STORE_H
