#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "gd32f4xx.h"
#include "bl_main.h"
#include "bl_setup.h"
#include "bl_flash_if.h"
#include "bl_parameters.h"
#include "bl_cmd_handler.h"
#include "frame_parser.h"
#include "frame_builder.h"
#include "CIMC_MCU.h"
#include "usart_app.h"
#include "systick.h"

/* ----------------------------------------------------------------------------
 * 常量与类型
 * ---------------------------------------------------------------------------- */

typedef void (*app_entry_t)(void);

/* APP 参数区（0x08010800）头部，BL 只读设备 ID 和波特率 */
#define APP_PARAM_FLASH_ADDR  0x08010800UL
#define APP_PARAM_MAGIC_VAL   0xA55AC33CUL

typedef struct
{
    uint32_t magic;
    uint16_t device_id;
    uint8_t  baud_code;
} bl_app_param_min_t;

/* 波特率编码  数值 */
static uint32_t baud_code_to_rate(uint8_t code)
{
    switch (code)
    {
    case 0x11U: return 4800U;
    case 0x12U: return 9600U;
    case 0x13U: return 19200U;
    case 0x14U: return 115200U;
    default:    return 19200U;
    }
}

/* ----------------------------------------------------------------------------
 * 内部函数
 * ---------------------------------------------------------------------------- */

/* 检查 APP 向量表是否合法：MSP 要在 SRAM 范围，Reset_Handler 要在 Flash 范围 */
static bool bl_is_app_vector_valid(uint32_t app_base)
{
    uint32_t msp           = *(volatile uint32_t *)app_base;
    uint32_t reset_handler = *(volatile uint32_t *)(app_base + 4UL);

    if ((msp < 0x20000100UL) || (msp > 0x20080000UL))
    {
        return false;
    }
    if ((reset_handler < BL_FLASH_BASE_ADDR) || (reset_handler > BL_FLASH_END_ADDR))
    {
        return false;
    }
    return true;
}

/* 跳转到 APP（不再返回）。跳转前要清理外设状态，否则 DMA 会乱写 APP 的内存 */
static void bl_jump_to_app(uint32_t app_base)
{
    /* 关 I2C0（OLED），让 APP 可以重新初始化 */
    i2c_disable(I2C0);
    gpio_mode_set(OLED_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  OLED_DAT_PIN | OLED_CLK_PIN);

    /* 关 USART1 DMA 和外设 */
    dma_channel_disable(OTA_UART_DMA, OTA_UART_DMA_CH);
    usart_dma_receive_config(RS232_RS485_USART, USART_RECEIVE_DMA_DISABLE);
    usart_disable(RS232_RS485_USART);

    __disable_irq();

    SysTick->CTRL = 0UL;
    SysTick->LOAD = 0UL;
    SysTick->VAL  = 0UL;

    for (uint32_t i = 0UL; i < 8UL; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFUL;
        NVIC->ICPR[i] = 0xFFFFFFFFUL;
    }

    __DSB();
    __ISB();

    SCB->VTOR = app_base;
    __set_MSP(*(volatile uint32_t *)app_base);

    app_entry_t app_entry = (app_entry_t)(*(volatile uint32_t *)(app_base + 4UL));
    __enable_irq();
    app_entry();
}

/* ============================================================================
 * BL 主流程
 *
 * 流程：读参数  判断升级/普通模式   执行对应路径
 * 升级模式：10秒倒计时  收固件  搬运   复位
 * 普通模式：静默5秒   跳APP
 * ============================================================================ */

void bl_execute(void)
{
    /* ---- 读参数页，判断启动模式 ---- */

    bl_param_t main_param;
    memcpy(&main_param, (const void *)BL_PARAM_MAIN_ADDR, sizeof(bl_param_t));

    bool is_upgrade_boot = (main_param.magic      == BL_PARAM_MAGIC     &&
                            main_param.tail_magic == BL_PARAM_TAIL_MAGIC &&
                            main_param.update_flag == BL_UPDATE_FLAG_PENDING);

    /* 从 APP 参数区读设备 ID（APP 在 L-01 可能改过 ID） */
    const bl_app_param_min_t *ap = (const bl_app_param_min_t *)APP_PARAM_FLASH_ADDR;
    uint16_t init_device_id = (ap->magic == APP_PARAM_MAGIC_VAL &&
                               ap->device_id != 0x0000U &&
                               ap->device_id != 0xFFFFU)
                              ? ap->device_id : 0x0001U;

    frame_parser_init();
    bl_cmd_handler_init(init_device_id);

    /* ---- 分叉 ---- */

    if (is_upgrade_boot)
    {
        /* 升级路径：10秒倒计时，等待上位机发 0x0502 */

        bl_cmd_handler_set_state(BL_UPGRADE_STATE_WAITING);

        /* 同步波特率：M-01 改过之后 BL 也得用同样的波特率 */
        {
            uint8_t bc = ap->baud_code;
            if (bc >= 0x11U && bc <= 0x14U)
            {
                uint32_t br = baud_code_to_rate(bc);
                usart_disable(RS232_RS485_USART);
                usart_baudrate_set(RS232_RS485_USART, br);
                usart_enable(RS232_RS485_USART);
            }
        }

        my_printf(rs485_uart, "using command to interrupt start Application\r\n");

        for (int remaining = 10; remaining > 0; remaining--)
        {
            if (remaining == 10 || remaining == 7 || remaining == 4 || remaining == 1)
            {
                my_printf(rs485_uart, "wait for start Application(%ds)......\r\n", remaining);
            }

            uint32_t elapsed = 0U;
            while (elapsed < 1000U)
            {
                delay_1ms(10);
                uart_task();
                elapsed += 10U;

                bl_upgrade_state_t st = bl_cmd_handler_get_state();
                if (st == BL_UPGRADE_STATE_RECEIVING || st == BL_UPGRADE_STATE_RECEIVED)
                {
                    goto upgrade_receive;
                }
                if (st == BL_UPGRADE_STATE_COMPLETE)
                {
                    goto upgrade_done;
                }
            }
        }

        /* 倒计时结束，跳 APP */
        if (bl_is_app_vector_valid(BL_APP1_START_ADDR))
        {
            my_printf(rs485_uart, "BL: jumping to app...\r\n");
            delay_1ms(100);
            bl_jump_to_app(BL_APP1_START_ADDR);
        }
    }
    else
    {
        /* 普通启动：静默 5 秒，赛题 2.7 要求不做任何输出 */
        delay_1ms(5000);

        if (bl_is_app_vector_valid(BL_APP1_START_ADDR))
        {
            bl_jump_to_app(BL_APP1_START_ADDR);
        }
    }

    /* ======================================================================
     * 固件接收
     *
     * 上位机发完 0x0502 后会直接发 bin 文件（不经帧封装，每片 256 字节）。
     * 这里不断收字节，连续 1.2 秒没收到新数据就认为传输完了。
     * 65 秒超时保底。
     *
     * 收完之后回复 0x0502 的结果（OK/ERROR），然后等 0x0503 来执行搬运。
     * 如果固件无效，等 12 秒让评测机重试（N-02 失败后接 N-03）。
     * ====================================================================== */

upgrade_receive:
    {
        uint32_t last_fw_count = 0U;
        uint32_t quiet_ms      = 0U;
        uint32_t total_ms      = 0U;
        bool     fw_started    = false;

        while (total_ms < 65000U)
        {
            delay_1ms(10U);
            uart_task();
            total_ms += 10U;

            uint32_t cur = bl_firmware_get_size();
            if (cur != last_fw_count)
            {
                last_fw_count = cur;
                quiet_ms   = 0U;
                fw_started = true;
            }
            else if (fw_started)
            {
                quiet_ms += 10U;
                if (quiet_ms >= 1200U)
                {
                    break;
                }
            }
        }

        bl_cmd_handler_set_state(BL_UPGRADE_STATE_IDLE);

        /* 回复 0x0502 验证结果 */
        {
            static char verify_resp[FRAME_ASCII_MAX];
            uint32_t vlen;

            if (bl_firmware_check_valid())
            {
                vlen = frame_build_ok(init_device_id, BL_CMD_UPGRADE_PREPARE, verify_resp);
            }
            else
            {
                /* 赛题 4.5.5(2)：错误应答用原命令字 0x0502 */
                vlen = frame_build_raw(init_device_id, FRAME_TYPE_ERROR,
                                       BL_CMD_UPGRADE_PREPARE, NULL, 0, verify_resp);
            }
            rs485_send_frame_pub(verify_resp, vlen);
        }

        /* 固件无效时等 12 秒让评测机重发 0x0502 */
        if (!bl_firmware_check_valid())
        {
            uint32_t retry_ms = 12000U;
            while (retry_ms > 0U)
            {
                delay_1ms(10U);
                uart_task();
                retry_ms -= 10U;
                if (bl_cmd_handler_get_state() == BL_UPGRADE_STATE_RECEIVING)
                {
                    goto upgrade_receive;
                }
            }
            goto idle_loop;
        }
    }

    /* ======================================================================
     * 等待 0x0503 执行升级（最多 12 秒）
     * ====================================================================== */

    {
        uint32_t wait_ms = 12000U;
        while (wait_ms > 0U)
        {
            delay_1ms(10U);
            uart_task();
            wait_ms -= 10U;
            if (bl_cmd_handler_get_state() == BL_UPGRADE_STATE_COMPLETE)
            {
                goto upgrade_done;
            }
        }
    }

    /* ======================================================================
     * 升级完成：复位。下次启动 PENDING 已清除，走普通路径进 APP
     * ====================================================================== */

upgrade_done:
    if (bl_cmd_handler_get_state() == BL_UPGRADE_STATE_COMPLETE)
    {
        delay_1ms(200U);
        NVIC_SystemReset();
    }

idle_loop:
    while (1)
    {
        delay_1ms(10U);
        uart_task();
        if (bl_cmd_handler_get_state() == BL_UPGRADE_STATE_RECEIVING)
        {
            goto upgrade_receive;
        }
    }
}
