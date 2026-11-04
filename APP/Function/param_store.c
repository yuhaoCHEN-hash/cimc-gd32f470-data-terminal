// 头文件
#include "param_store.h"
#include "gd32f4xx.h"       // fmc_unlock / fmc_lock / fmc_page_erase / fmc_word_program
#include <string.h>
#include <stddef.h>         // offsetof

//
// 整页（4KB）RAM 缓冲区，用于读改写操作。
// 所有 Flash 写操作（param + alarm）共用此唯一缓冲区，串行执行，不存在双缓冲竞争。
//
static uint8_t s_page_buf[APP_PARAM_PAGE_SIZE];

// 函数定义

//
// Comment  :       CRC32 计算（IEEE 802.3）
// Parameter:       data - 数据指针
//                  len  - 数据长度
// Return   :       CRC32 校验值
//
static uint32_t param_crc32_calc(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    for (uint32_t i = 0U; i < len; i++)
    {
        crc ^= data[i];
        for (uint32_t j = 0U; j < 8U; j++)
        {
            if (crc & 1UL)
                crc = (crc >> 1U) ^ 0xEDB88320UL;
            else
                crc >>= 1U;
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

//
// Comment  :       擦除页并按 word 回写整页数据（RAM 中执行）
// Parameter:       page_addr - 页起始地址
//                  buf       - 待写入数据缓冲区
// Return   :       true  - 成功
//                  false - 失败
//
__attribute__((section(".ramfunc"))) static bool flash_page_rewrite(uint32_t page_addr, const uint8_t *buf)
{
    const uint32_t *words = (const uint32_t *)buf;
    uint32_t word_count   = APP_PARAM_PAGE_SIZE / 4U;
    uint32_t addr         = page_addr;
    fmc_state_enum st;

    fmc_unlock();

    st = fmc_page_erase(page_addr);
    if (st != FMC_READY)
    {
        fmc_lock();
        return false;
    }
    st = fmc_page_erase(page_addr + 0x800U);
    if (st != FMC_READY)
    {
        fmc_lock();
        return false;
    }

    for (uint32_t i = 0U; i < word_count; i++)
    {
        st = fmc_word_program(addr, words[i]);
        if (st != FMC_READY)
        {
            fmc_lock();
            return false;
        }
        addr += 4U;
    }

    fmc_lock();
    return true;
}

//
// Comment  :       通用 Flash 页局部更新（所有 Flash 写的唯一入口）
// Parameter:       page_offset - 页内字节偏移
//                  data        - 待写入数据
//                  len         - 数据长度
// Return   :       true  - 成功
//                  false - 参数错误或写入失败
//
bool flash_page_partial_update(uint32_t page_offset, const void *data, uint32_t len)
{
    if (data == NULL)
    {
        return false;
    }
    if (page_offset + len > APP_PARAM_PAGE_SIZE)
    {
        return false;
    }

    const uint8_t *flash_page = (const uint8_t *)APP_PARAM_FLASH_PAGE;
    memcpy(s_page_buf, flash_page, APP_PARAM_PAGE_SIZE);
    memcpy(&s_page_buf[page_offset], data, len);
    return flash_page_rewrite(APP_PARAM_FLASH_PAGE, s_page_buf);
}

//
// Comment  :       从 Flash 加载 APP 参数
// Parameter:       out - 输出参数结构体指针
// Return   :       true  - Flash 有有效参数
//                  false - 参数无效或首次上电
//
bool param_load(app_param_t *out)
{
    if (out == NULL)
    {
        return false;
    }

    const app_param_t *stored = (const app_param_t *)APP_PARAM_FLASH_ADDR;

    if (stored->magic != APP_PARAM_MAGIC)
    {
        return false;
    }
    if (stored->device_id == 0x0000U || stored->device_id == 0xFFFFU)
    {
        return false;
    }
    if (stored->baud_code < 0x11U || stored->baud_code > 0x14U)
    {
        return false;
    }

    uint32_t expected = param_crc32_calc((const uint8_t *)stored,
                                         offsetof(app_param_t, crc32));
    if (expected != stored->crc32)
    {
        return false;
    }

    *out = *stored;
    return true;
}

//
// Comment  :       保存 APP 参数到 Flash
// Parameter:       p - 待保存的参数结构体指针
// Return   :       true  - 成功
//                  false - 写入失败
//
bool param_save(const app_param_t *p)
{
    if (p == NULL)
    {
        return false;
    }

    app_param_t tmp = *p;
    tmp.crc32 = param_crc32_calc((const uint8_t *)&tmp,
                                  offsetof(app_param_t, crc32));

    uint32_t offset = APP_PARAM_FLASH_ADDR - APP_PARAM_FLASH_PAGE;
    return flash_page_partial_update(offset, &tmp, sizeof(app_param_t));
}

//
// Comment  :       设置升级 PENDING 标志（0x0501 复位前调用）
// Parameter:       none
// Return   :       true  - 成功
//                  false - 写入失败
//
bool param_set_upgrade_pending(void)
{
    // BL param struct layout offsets (bl_param_t, 256 bytes)
#define BL_MAGIC_OFFSET        0U
#define BL_VERSION_OFFSET      4U
#define BL_UPDATE_FLAG_OFFSET  8U
#define BL_TAIL_MAGIC_OFFSET   252U

#define BL_PARAM_MAGIC_VAL     0x5AA5C33CUL
#define BL_PARAM_TAIL_MAGIC_V  0xA5A5C3C3UL
#define BL_PARAM_VERSION_VAL   0x00010001UL
#define BL_UPDATE_FLAG_PENDING 0xAA55AA55UL

    const uint8_t *flash_page = (const uint8_t *)APP_PARAM_FLASH_PAGE;
    memcpy(s_page_buf, flash_page, APP_PARAM_PAGE_SIZE);

    uint32_t *p = (uint32_t *)&s_page_buf[0];
    for (uint32_t i = 0U; i < 64U; i++) p[i] = 0U;

    p[BL_MAGIC_OFFSET       / 4U] = BL_PARAM_MAGIC_VAL;
    p[BL_VERSION_OFFSET     / 4U] = BL_PARAM_VERSION_VAL;
    p[BL_UPDATE_FLAG_OFFSET / 4U] = BL_UPDATE_FLAG_PENDING;
    p[BL_TAIL_MAGIC_OFFSET  / 4U] = BL_PARAM_TAIL_MAGIC_V;

    return flash_page_rewrite(APP_PARAM_FLASH_PAGE, s_page_buf);
}
