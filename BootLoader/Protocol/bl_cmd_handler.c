#include "bl_cmd_handler.h"
#include "frame_builder.h"
#include "frame_parser.h"
#include "crc16_modbus.h"
#include "bl_layout.h"
#include "bl_parameters.h"
#include "CIMC_MCU.h"
#include <string.h>
#include <stddef.h>

/* ---------------------------------------------------------------------- */
/* 内部常量                                                               */
/* ---------------------------------------------------------------------- */

/* 固件.bin的魔术字：.bin 文件前 4 字节 */
#define FW_MAGIC_B0  0x5AU
#define FW_MAGIC_B1  0xA5U
#define FW_MAGIC_B2  0xC3U
#define FW_MAGIC_B3  0x3CU

/* 逐字节检查魔术字 */
#define IS_MAGIC_OK(p)  ((p)[0] == FW_MAGIC_B0 && (p)[1] == FW_MAGIC_B1 && \
                         (p)[2] == FW_MAGIC_B2 && (p)[3] == FW_MAGIC_B3)

/* ---------------------------------------------------------------------- */
/* 模块状态                                                               */
/* ---------------------------------------------------------------------- */

static uint16_t          s_bl_device_id = 0x0001;
static bl_upgrade_state_t s_state       = BL_UPGRADE_STATE_IDLE;

static uint32_t s_fw_size     = 0U;
static uint32_t s_fw_crc32    = 0xFFFFFFFFUL;
static uint32_t s_fw_received = 0U;
static bool     s_fw_valid    = true;

/* ---------------------------------------------------------------------- */
/* CRC32（IEEE 802.3）                                                    */
/* ---------------------------------------------------------------------- */

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0U; i < len; i++)
    {
        crc ^= data[i];
        for (uint32_t j = 0U; j < 8U; j++)
        {
            if (crc & 1U)
            {
                crc = (crc >> 1U) ^ 0xEDB88320UL;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }
    return crc;
}

static uint32_t crc32_calc(const uint8_t *data, uint32_t len)
{
    return crc32_update(0xFFFFFFFFUL, data, len) ^ 0xFFFFFFFFUL;
}

/* ---------------------------------------------------------------------- */
/* 片内 Flash 擦写辅助（.ramfunc 区）                                     */
/* ---------------------------------------------------------------------- */

__attribute__((section(".ramfunc"), noinline))
static bool flash_wait_ready(void)
{
    uint32_t timeout = 0x3FFFFFUL;
    while ((RESET != fmc_flag_get(FMC_FLAG_BUSY)) && (timeout > 0U))
    {
        timeout--;
    }
    return (timeout > 0U);
}

__attribute__((section(".ramfunc"), noinline))
static void flash_clear_flags(void)
{
    fmc_flag_clear(FMC_FLAG_END);
    fmc_flag_clear(FMC_FLAG_WPERR);
    fmc_flag_clear(FMC_FLAG_PGSERR);
    fmc_flag_clear(FMC_FLAG_PGMERR);
}

__attribute__((section(".ramfunc"), noinline))
static bool flash_erase_pages(uint32_t start_addr, uint32_t size)
{
    if ((size == 0U) || (start_addr < BL_FLASH_BASE_ADDR))
    {
        return false;
    }

    uint32_t end_addr = start_addr + size - 1U;
    if (end_addr > BL_FLASH_END_ADDR)
    {
        return false;
    }

    uint32_t page_addr = start_addr - (start_addr % BL_FLASH_PAGE_SIZE);
    end_addr = end_addr - (end_addr % BL_FLASH_PAGE_SIZE);

    fmc_unlock();
    flash_clear_flags();

    while (page_addr <= end_addr)
    {
        fmc_page_erase(page_addr);
        if (!flash_wait_ready())
        {
            fmc_lock();
            return false;
        }
        flash_clear_flags();
        page_addr += BL_FLASH_PAGE_SIZE;
    }

    fmc_lock();
    return true;
}

__attribute__((section(".ramfunc"), noinline))
static bool flash_program_bytes(uint32_t addr, const uint8_t *data, uint32_t size)
{
    uint32_t word_value;

    if ((data == NULL) || (size == 0U))
    {
        return false;
    }
    if ((addr < BL_FLASH_BASE_ADDR) || ((addr + size - 1U) > BL_FLASH_END_ADDR))
    {
        return false;
    }

    fmc_unlock();
    flash_clear_flags();

    /* word 对齐写入 */
    while (((addr & 3UL) == 0UL) && (size >= 4U))
    {
        memcpy(&word_value, data, sizeof(word_value));
        fmc_word_program(addr, word_value);
        if (!flash_wait_ready())
        {
            fmc_lock();
            return false;
        }
        addr += 4UL; data += 4U; size -= 4U;
    }

    /* 尾部字节写入 */
    for (uint32_t i = 0U; i < size; i++)
    {
        fmc_byte_program(addr + i, data[i]);
        if (!flash_wait_ready())
        {
            fmc_lock();
            return false;
        }
    }

    fmc_lock();
    return true;
}

/* ---------------------------------------------------------------------- */
/* 公开 API                                                               */
/* ---------------------------------------------------------------------- */

void bl_cmd_handler_init(uint16_t device_id)
{
    s_bl_device_id = device_id;
    s_state = BL_UPGRADE_STATE_IDLE;
    bl_firmware_reset();
}

uint32_t bl_cmd_handler_process(const protocol_frame_t *req, char *out_ascii)
{
    if (req == NULL || out_ascii == NULL)
    {
        return 0;
    }

    /* 过滤非本机地址 */
    if (req->device_id != s_bl_device_id && req->device_id != 0xFFFF)
    {
        return 0;
    }

    /* 广播搜索 → 心跳帧 */
    if (req->frame_type == 0x05 && req->command == 0xFFFF)
    {
        return frame_build_heartbeat(s_bl_device_id, out_ascii);
    }

    if (req->frame_type != 0x01)
    {
        return 0;
    }

    switch (req->command)
    {
    case BL_CMD_UPGRADE_PREPARE:    /* 0x0502 */
        s_state = BL_UPGRADE_STATE_RECEIVING;
        bl_firmware_reset();
        return 0U;  /* 不应答：由 bl_core.c 在接收完毕后统一回复 */

    case BL_CMD_UPGRADE_EXECUTE:    /* 0x0503 */
        if (!s_fw_valid || s_fw_received < 4U)
        {
            return frame_build_error(s_bl_device_id, out_ascii);
        }
        if (!IS_MAGIC_OK((const uint8_t *)BL_APP2_START_ADDR))
        {
            return frame_build_error(s_bl_device_id, out_ascii);
        }
        s_state = BL_UPGRADE_STATE_COMPLETE;
        return frame_build_ok(s_bl_device_id, BL_CMD_UPGRADE_EXECUTE, out_ascii);

    default:
        return frame_build_error(s_bl_device_id, out_ascii);
    }
}

bl_upgrade_state_t bl_cmd_handler_get_state(void)
{
    return s_state;
}

void bl_cmd_handler_set_state(bl_upgrade_state_t state)
{
    s_state = state;
}

/* ---------------------------------------------------------------------- */
/* 固件接收                                                               */
/* ---------------------------------------------------------------------- */

void bl_firmware_feed(const uint8_t *data, uint32_t len)
{
    if (s_state != BL_UPGRADE_STATE_RECEIVING)
    {
        return;
    }
    if (data == NULL || len == 0U)
    {
        return;
    }

    /* 第一片：校验魔术字 */
    if (s_fw_valid && s_fw_received == 0U && len >= 4U)
    {
        if (!IS_MAGIC_OK(data))
        {
            s_fw_valid = false;
        }
    }

    /* 魔术字无效：仅计数，不写 Flash */
    if (!s_fw_valid)
    {
        s_fw_received += len;
        return;
    }

    /* 容量检查 */
    if (s_fw_received + len > BL_APP2_SIZE)
    {
        s_fw_valid = false;
        s_fw_received += len;
        return;
    }

    /* 首片数据时擦除 APP2 */
    if (s_fw_received == 0U)
    {
        if (!flash_erase_pages(BL_APP2_START_ADDR, BL_APP2_SIZE))
        {
            s_fw_valid = false;
            s_fw_received += len;
            return;
        }
    }

    /* 写入 Flash */
    if (!flash_program_bytes(BL_APP2_START_ADDR + s_fw_received, data, len))
    {
        s_fw_valid = false;
        s_fw_received += len;
        return;
    }

    /* 累计 CRC（跳过魔术字 4 字节） */
    {
        uint32_t skip = (s_fw_received < 4U) ? (4U - s_fw_received) : 0U;
        if (skip < len)
        {
            s_fw_crc32 = crc32_update(s_fw_crc32, data + skip, len - skip);
        }
    }
    s_fw_received += len;
}

bool bl_firmware_is_complete(void)
{
    return s_fw_valid && (s_fw_received > 0U);
}

bool bl_firmware_check_valid(void)
{
    return s_fw_valid && (s_fw_received >= 4U);
}

uint32_t bl_firmware_get_size(void)
{
    return s_fw_received;
}

uint32_t bl_firmware_get_crc32(void)
{
    return s_fw_crc32 ^ 0xFFFFFFFFUL;
}

void bl_firmware_reset(void)
{
    s_fw_size     = 0U;
    s_fw_crc32    = 0xFFFFFFFFUL;
    s_fw_received = 0U;
    s_fw_valid    = true;
}

/* ---------------------------------------------------------------------- */
/* 固件搬运：APP2 → APP1                                                  */
/* ---------------------------------------------------------------------- */

bool bl_firmware_validate_and_copy(void)
{
    uint8_t buffer[256];
    uint32_t final_crc;

    if (s_fw_received == 0U || !s_fw_valid)
    {
        return false;
    }

    final_crc = s_fw_crc32 ^ 0xFFFFFFFFUL;

    if (!IS_MAGIC_OK((const uint8_t *)BL_APP2_START_ADDR))
    {
        return false;
    }

    uint32_t fw_size = s_fw_received - 4U;
    if (fw_size == 0U)
    {
        return false;
    }

    /* 擦除 APP1 */
    uint32_t erase_size = (fw_size + BL_FLASH_PAGE_SIZE - 1UL) & ~(BL_FLASH_PAGE_SIZE - 1UL);
    if (!flash_erase_pages(BL_APP1_START_ADDR, erase_size))
    {
        return false;
    }

    /* 从 APP2+4 拷贝到 APP1（跳过魔术字） */
    uint32_t copied = 0U;
    while (copied < fw_size)
    {
        uint32_t left  = fw_size - copied;
        uint32_t chunk = (left > 256U) ? 256U : left;
        memcpy(buffer, (const uint8_t *)(BL_APP2_START_ADDR + 4U + copied), chunk);
        if (!flash_program_bytes(BL_APP1_START_ADDR + copied, buffer, chunk))
        {
            return false;
        }
        copied += chunk;
    }

    /* 校验 APP1 CRC */
    uint32_t app1_crc = crc32_calc((const uint8_t *)BL_APP1_START_ADDR, fw_size);
    if (app1_crc != final_crc)
    {
        return false;
    }

    bl_params_write(NULL);
    return true;
}

/* ---------------------------------------------------------------------- */
/* 清除 PENDING 标志                                                       */
/* ---------------------------------------------------------------------- */

bool bl_params_write(bl_param_t *param)
{
    (void)param;
    static uint8_t buf[BL_PARAM_PAGE_SIZE];

    memcpy(buf, (const void *)BL_PARAM_PAGE_ADDR, BL_PARAM_PAGE_SIZE);

    bl_param_t *main_p = (bl_param_t *)buf;
    main_p->update_flag = BL_UPDATE_FLAG_IDLE;

    {
        uint32_t crc_len = (uint32_t)((const uint8_t *)&main_p->param_crc32 -
                                      (const uint8_t *)main_p);
        main_p->param_crc32 = crc32_calc((const uint8_t *)main_p, crc_len);
    }

    if (!flash_erase_pages(BL_PARAM_PAGE_ADDR, BL_PARAM_PAGE_SIZE))
    {
        return false;
    }
    return flash_program_bytes(BL_PARAM_PAGE_ADDR, buf, BL_PARAM_PAGE_SIZE);
}
