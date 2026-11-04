// 头文件
#include "cimc_f470vet6.h"
#include "usart_app.h"
#include "ota_uart.h"
#include "ring_buffer.h"
#include "bl_partition.h"
#include "bl_param.h"
#include "ota_protocol.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// 宏定义
#define OTA_RING_BUFFER_SIZE        (16 * 1024)

// 变量定义
typedef enum
{
    OTA_STATE_WAIT_MAGIC = 0,
    OTA_STATE_RECV_HEADER,
    OTA_STATE_RECV_RAWDATA
} ota_raw_state_t;

typedef struct
{
    ota_raw_state_t state;
    uint32_t app_version;
    uint32_t expected_size;
    uint32_t expected_crc32;
    uint32_t recv_size;
    uint32_t crc_acc;
    uint8_t  header_buf[sizeof(ota_header_t)];
    uint32_t header_pos;
    uint32_t last_progress_print;
} ota_raw_ctx_t;

static ota_raw_ctx_t s_ota_ctx;

static uint8_t s_param_page_cache[BL_PARAM_PAGE_SIZE];
static uint8_t s_ring_buffer_storage[OTA_RING_BUFFER_SIZE];
static ring_buffer_t s_ring_buffer;

// 函数定义

/************************************************************
 * Comment  :       CRC32 更新（查表法，多项式 0xEDB88320）
 * Parameter:       crc  - 当前 CRC 累加值
 *                  data - 数据指针
 *                  len  - 数据长度
 * Return   :       更新后的 CRC 值
************************************************************/
static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    static const uint32_t t[16] =
    {
        0x00000000UL, 0x1DB71064UL, 0x3B6E20C8UL, 0x26D930ACUL,
        0x76DC4190UL, 0x6B6B51F4UL, 0x4DB26158UL, 0x5005713CUL,
        0xEDB88320UL, 0xF00F9344UL, 0xD6D6A3E8UL, 0xCB61B38CUL,
        0x9B64C2B0UL, 0x86D3D2D4UL, 0xA00AE278UL, 0xBDBDF21CUL
    };
    for (uint32_t i = 0U; i < len; i++)
    {
        crc ^= data[i];
        crc = (crc >> 4U) ^ t[crc & 0x0FU];
        crc = (crc >> 4U) ^ t[crc & 0x0FU];
    }
    return crc;
}

static uint32_t crc32_finalize(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFUL;
}

static uint32_t crc32_calc(const uint8_t *data, uint32_t len)
{
    return crc32_finalize(crc32_update(0xFFFFFFFFUL, data, len));
}

static uint32_t param_calc_crc(const bl_param_t *param)
{
    return crc32_calc((const uint8_t *)param, (uint32_t)offsetof(bl_param_t, param_crc32));
}

/************************************************************
 * Comment  :       OTA 安全打印（下载期间抑制，仅打印结果）
 * Parameter:       format - 格式字符串
 *                  ...    - 可变参数
 * Return   :       none
************************************************************/
static void ota_safe_printf(const char *format, ...)
{
    char buffer[256];
    va_list arg;
    int len;

    va_start(arg, format);
    len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    if (len <= 0)
        return;

    RS485_CS_SET(1);
    for (int i = 0; i < len; i++)
    {
        usart_data_transmit(USART1, (uint8_t)buffer[i]);
        while (RESET == usart_flag_get(USART1, USART_FLAG_TBE));
    }
    while (RESET == usart_flag_get(USART1, USART_FLAG_TC));
    RS485_CS_SET(0);
}

static void ota_safe_delay(void)
{
    for (volatile uint32_t d = 0U; d < 100000U; d++) { __NOP(); }
}

/* ---- RAM 执行辅助（FMC 操作需要运行在 RAM 中） ---- */

__attribute__((section(".ramfunc")))
static void fmc_clear_error_flags(void)
{
    fmc_flag_clear(FMC_FLAG_END);
    fmc_flag_clear(FMC_FLAG_WPERR);
    fmc_flag_clear(FMC_FLAG_PGSERR);
    fmc_flag_clear(FMC_FLAG_PGMERR);
}

__attribute__((section(".ramfunc")))
static bool fmc_wait_busy(void)
{
    uint32_t timeout = 0x3FFFFFUL;
    while ((fmc_flag_get(FMC_FLAG_BUSY) != RESET) && (timeout > 0U))
        timeout--;
    return (timeout > 0U);
}

/************************************************************
 * Comment  :       擦除 Flash 页（RAM 中执行，避免总线冲突）
 * Parameter:       addr - 起始地址
 *                  size - 擦除大小
 * Return   :       true  - 成功
 *                  false - 失败
************************************************************/
__attribute__((section(".ramfunc"), noinline))
static bool flash_erase_pages(uint32_t addr, uint32_t size)
{
    uint32_t pg, end;

    if ((size == 0U) || (addr < BL_FLASH_BASE_ADDR) || (addr + size > BL_FLASH_END_ADDR + 1U))
        return false;

    pg  = addr - (addr % BL_FLASH_PAGE_SIZE);
    end = (addr + size - 1U) - ((addr + size - 1U) % BL_FLASH_PAGE_SIZE);

    fmc_unlock();
    fmc_clear_error_flags();

    while (pg <= end)
    {
        fmc_page_erase(pg);

        if (!fmc_wait_busy())
        {
            fmc_lock();
            return false;
        }

        fmc_clear_error_flags();

        pg += BL_FLASH_PAGE_SIZE;
    }

    fmc_lock();
    return true;
}

/************************************************************
 * Comment  :       编程 Flash（RAM 中执行）
 * Parameter:       addr - 写入地址
 *                  data - 数据指针
 *                  size - 数据大小
 * Return   :       true  - 成功
 *                  false - 失败
************************************************************/
__attribute__((section(".ramfunc"), noinline))
static bool flash_program_bytes(uint32_t addr, const uint8_t *data, uint32_t size)
{
    uint32_t i, tmp;

    if ((data == NULL) || (size == 0U))
        return false;
    if ((addr < BL_FLASH_BASE_ADDR) || ((addr + size - 1U) > BL_FLASH_END_ADDR))
        return false;

    fmc_unlock();
    fmc_clear_error_flags();

    while ((addr & 3UL) == 0UL && size >= 4U)
    {
        memcpy(&tmp, data, sizeof(tmp));
        fmc_word_program(addr, tmp);
        if (!fmc_wait_busy()) { fmc_lock(); return false; }
        fmc_clear_error_flags();
        addr += 4UL;
        data += 4U;
        size -= 4U;
    }

    for (i = 0U; i < size; i++)
    {
        fmc_byte_program(addr + i, data[i]);
        if (!fmc_wait_busy()) { fmc_lock(); return false; }
        fmc_clear_error_flags();
    }

    fmc_lock();
    return true;
}

static bool param_check_valid(const bl_param_t *p)
{
    return (p->magic == BL_PARAM_MAGIC) &&
           (p->tail_magic == BL_PARAM_TAIL_MAGIC) &&
           (p->version == BL_PARAM_VERSION);
}

static void param_fill_default(bl_param_t *p)
{
    memset(p, 0, sizeof(bl_param_t));
    p->magic = BL_PARAM_MAGIC;
    p->version = BL_PARAM_VERSION;
    p->update_flag = BL_UPDATE_FLAG_IDLE;
    p->app1_addr = BL_APP1_START_ADDR;
    p->app2_addr = BL_APP2_START_ADDR;
    p->tail_magic = BL_PARAM_TAIL_MAGIC;
    p->param_crc32 = param_calc_crc(p);
}

/************************************************************
 * Comment  :       提交 OTA 更新参数到 BL 参数页
 * Parameter:       app_size - 固件大小
 *                  app_crc32 - 固件 CRC32
 * Return   :       true  - 成功
 *                  false - 失败
************************************************************/
static bool param_commit_update(uint32_t app_size, uint32_t app_crc32)
{
    bl_param_t main_p, back_p;

    memcpy(s_param_page_cache, (const void *)BL_PARAM_PAGE_ADDR, BL_PARAM_PAGE_SIZE);
    memcpy(&main_p, (const void *)BL_PARAM_MAIN_ADDR, sizeof(bl_param_t));
    memcpy(&back_p, (const void *)BL_PARAM_BACKUP_ADDR, sizeof(bl_param_t));

    if (!param_check_valid(&main_p))
    {
        if (param_check_valid(&back_p))
            main_p = back_p;
        else
            param_fill_default(&main_p);
    }

    main_p.app_size = app_size;
    main_p.app_crc32 = app_crc32;
    main_p.app1_addr = BL_APP1_START_ADDR;
    main_p.app2_addr = BL_APP2_START_ADDR;
    main_p.update_flag = BL_UPDATE_FLAG_PENDING;
    main_p.last_error = BL_ERR_NONE;
    main_p.param_crc32 = param_calc_crc(&main_p);
    back_p = main_p;

    {
        uint32_t off_m = BL_PARAM_MAIN_ADDR - BL_PARAM_PAGE_ADDR;
        uint32_t off_b = BL_PARAM_BACKUP_ADDR - BL_PARAM_PAGE_ADDR;
        memcpy(&s_param_page_cache[off_m], &main_p, sizeof(bl_param_t));
        memcpy(&s_param_page_cache[off_b], &back_p, sizeof(bl_param_t));
    }

    if (!flash_erase_pages(BL_PARAM_PAGE_ADDR, BL_PARAM_PAGE_SIZE))
        return false;
    return flash_program_bytes(BL_PARAM_PAGE_ADDR, s_param_page_cache, BL_PARAM_PAGE_SIZE);
}

/************************************************************
 * Comment  :       复位 OTA 状态机
 * Parameter:       none
 * Return   :       none
************************************************************/
void ota_uart_reset_state(void)
{
    memset(&s_ota_ctx, 0, sizeof(s_ota_ctx));
    s_ota_ctx.state = OTA_STATE_WAIT_MAGIC;
    s_ota_ctx.crc_acc = 0xFFFFFFFFUL;
    ring_buffer_init(&s_ring_buffer, s_ring_buffer_storage, OTA_RING_BUFFER_SIZE);
}

/************************************************************
 * Comment  :       查询 OTA 是否正在接收数据
 * Parameter:       none
 * Return   :       true - 正在接收固件数据
************************************************************/
bool ota_uart_is_active(void)
{
    return (s_ota_ctx.state == OTA_STATE_RECV_RAWDATA);
}

/************************************************************
 * Comment  :       处理 OTA 数据帧
 * Parameter:       data - 帧数据
 *                  len  - 数据长度
 * Return   :       none
************************************************************/
void ota_uart_process_frame(const uint8_t *data, uint32_t len)
{
    if ((data == NULL) || (len == 0U))
        return;

    if (!ring_buffer_write(&s_ring_buffer, data, len))
    {
        ota_uart_reset_state();
    }
}

/************************************************************
 * Comment  :       开始 OTA 更新
 * Parameter:       header - OTA 头部信息
 * Return   :       true  - 成功
 *                  false - 参数错误
************************************************************/
static bool ota_begin(const ota_header_t *header)
{
    uint32_t erase_size;

    if ((header->app_size == 0U) || (header->app_size > BL_APP2_SIZE) || (header->magic != OTA_FRAME_MAGIC))
        return false;

    erase_size = (header->app_size + BL_FLASH_PAGE_SIZE - 1U) & ~(BL_FLASH_PAGE_SIZE - 1U);
    if (!flash_erase_pages(BL_APP2_START_ADDR, erase_size))
        return false;

    s_ota_ctx.state = OTA_STATE_RECV_RAWDATA;
    s_ota_ctx.app_version = header->app_version;
    s_ota_ctx.expected_size = header->app_size;
    s_ota_ctx.expected_crc32 = header->app_crc32;
    s_ota_ctx.recv_size = 0U;
    s_ota_ctx.crc_acc = 0xFFFFFFFFUL;
    s_ota_ctx.last_progress_print = 0U;

    return true;
}

/************************************************************
 * Comment  :       写入原始固件数据到 APP2
 * Parameter:       data - 固件数据
 *                  len  - 数据长度
 * Return   :       true  - 成功
 *                  false - 失败
************************************************************/
static bool ota_write_raw_data(const uint8_t *data, uint32_t len)
{
    uint32_t flash_addr;

    if ((data == NULL) || (len == 0U))
        return true;

    if ((s_ota_ctx.recv_size + len) > s_ota_ctx.expected_size)
        return false;

    flash_addr = BL_APP2_START_ADDR + s_ota_ctx.recv_size;

    if (!flash_program_bytes(flash_addr, data, len))
        return false;

    s_ota_ctx.crc_acc = crc32_update(s_ota_ctx.crc_acc, data, len);
    s_ota_ctx.recv_size += len;
    return true;
}

/************************************************************
 * Comment  :       完成 OTA 更新：校验 CRC 并复位
 * Parameter:       none
 * Return   :       none
************************************************************/
static void ota_finalize(void)
{
    uint32_t crc_value = crc32_finalize(s_ota_ctx.crc_acc);

    if (crc_value != s_ota_ctx.expected_crc32)
    {
        ota_safe_printf("Magic OK: 0x%08X, Ver: 0x%08X\r\n", OTA_FRAME_MAGIC, s_ota_ctx.app_version);
        ota_safe_delay();
        ota_safe_printf("Received: %u, CRC: 0x%08X (EXP: 0x%08X)\r\n", s_ota_ctx.recv_size, crc_value, s_ota_ctx.expected_crc32);
        ota_safe_delay();
        ota_safe_printf("CRC FAIL\r\n");
        ota_uart_reset_state();
        return;
    }

    ota_safe_printf("Magic OK: 0x%08X, Ver: 0x%08X\r\n", OTA_FRAME_MAGIC, s_ota_ctx.app_version);
    ota_safe_delay();
    ota_safe_printf("Raw OTA: start, size=%u, crc=0x%08X\r\n", s_ota_ctx.expected_size, s_ota_ctx.expected_crc32);
    ota_safe_delay();
    ota_safe_printf("Received: %u, CRC: 0x%08X\r\n", s_ota_ctx.recv_size, crc_value);
    ota_safe_delay();
    ota_safe_printf("CRC PASS\r\n");
    ota_safe_delay();

    if (!param_commit_update(s_ota_ctx.expected_size, s_ota_ctx.expected_crc32))
    {
        ota_safe_printf("Param commit FAILED\r\n");
        ota_uart_reset_state();
        return;
    }

    ota_safe_printf("Update SUCCESS!!\r\n");

    for (volatile int i = 0; i < 100000; i++) { __NOP(); }
    NVIC_SystemReset();
}

/************************************************************
 * Comment  :       OTA UART 任务：状态机处理固件接收
 * Parameter:       none
 * Return   :       none
************************************************************/
void ota_uart_task(void)
{
    uint8_t byte;
    uint32_t available;
    uint32_t chunk;
    uint8_t chunk_buf[512];

    while (1)
    {
        available = ring_buffer_available(&s_ring_buffer);

        if (s_ota_ctx.state == OTA_STATE_WAIT_MAGIC)
        {
            if (available < 1U)
                return;

            (void)ring_buffer_peek(&s_ring_buffer, &byte, 1U);
            if (byte != (uint8_t)(OTA_FRAME_MAGIC & 0xFFU))
            {
                ring_buffer_drop(&s_ring_buffer, 1U);
                continue;
            }

            if (available < 4U)
                return;

            uint8_t peek4[4];
            (void)ring_buffer_peek(&s_ring_buffer, peek4, 4U);
            uint32_t candidate = ((uint32_t)peek4[3] << 24) | ((uint32_t)peek4[2] << 16) |
                                ((uint32_t)peek4[1] << 8)  |  (uint32_t)peek4[0];
            if (candidate != OTA_FRAME_MAGIC)
            {
                ring_buffer_drop(&s_ring_buffer, 1U);
                continue;
            }

            if (available < sizeof(ota_header_t))
                return;

            ring_buffer_drop(&s_ring_buffer, 4U);
            s_ota_ctx.header_pos = 0U;
            s_ota_ctx.state = OTA_STATE_RECV_HEADER;
        }

        if (s_ota_ctx.state == OTA_STATE_RECV_HEADER)
        {
            uint32_t need = sizeof(ota_header_t) - 4U - s_ota_ctx.header_pos;
            available = ring_buffer_available(&s_ring_buffer);
            if (available < need)
                return;

            uint32_t rd = ring_buffer_read(&s_ring_buffer,
                                           &s_ota_ctx.header_buf[4U + s_ota_ctx.header_pos], need);
            s_ota_ctx.header_pos += rd;

            if (s_ota_ctx.header_pos < (sizeof(ota_header_t) - 4U))
                return;

            s_ota_ctx.header_buf[0] = (uint8_t)(OTA_FRAME_MAGIC & 0xFFU);
            s_ota_ctx.header_buf[1] = (uint8_t)((OTA_FRAME_MAGIC >> 8) & 0xFFU);
            s_ota_ctx.header_buf[2] = (uint8_t)((OTA_FRAME_MAGIC >> 16) & 0xFFU);
            s_ota_ctx.header_buf[3] = (uint8_t)((OTA_FRAME_MAGIC >> 24) & 0xFFU);

            ota_header_t header;
            memcpy(&header, s_ota_ctx.header_buf, sizeof(header));

            if (!ota_begin(&header))
            {
                ota_uart_reset_state();
                return;
            }
        }

        if (s_ota_ctx.state == OTA_STATE_RECV_RAWDATA)
        {
            available = ring_buffer_available(&s_ring_buffer);
            if (available == 0U)
                return;

            uint32_t remaining = s_ota_ctx.expected_size - s_ota_ctx.recv_size;
            chunk = (available < remaining) ? available : remaining;
            if (chunk > sizeof(chunk_buf))
                chunk = sizeof(chunk_buf);

            uint32_t rd = ring_buffer_read(&s_ring_buffer, chunk_buf, chunk);
            if (rd == 0U)
                return;

            if (!ota_write_raw_data(chunk_buf, rd))
            {
                ota_safe_printf("Raw OTA: write failed at %u/%u\r\n",
                                s_ota_ctx.recv_size, s_ota_ctx.expected_size);
                ota_uart_reset_state();
                return;
            }

            if (s_ota_ctx.recv_size >= s_ota_ctx.expected_size)
            {
                ota_finalize();
                return;
            }
        }
    }
}
