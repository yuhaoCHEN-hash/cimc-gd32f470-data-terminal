// 头文件
#include "cimc_f470vet6.h"
#include "frame_parser.h"
#include "cmd_handler.h"

// 宏定义
#define TEAM_DEVICE_ID  0x0001

// 函数定义

/************************************************************
 * Comment  :       系统主入口：初始化各模块后进入调度器循环
 * Parameter:       none
 * Return   :       none
************************************************************/
int main(void)
{
#ifdef __FIRMWARE_VERSION_DEFINE
    uint32_t fw_ver = 0;
#endif
    systick_config();
    init_cycle_counter(false);
    delay_ms(200);

#ifdef __FIRMWARE_VERSION_DEFINE
    fw_ver = gd32f4xx_firmware_version_get();
#endif

    bsp_led_init();
    bsp_btn_init();
    bsp_oled_init();
    bsp_gd25qxx_init();
    bsp_usart_init();

    bsp_gd30ad3344_init();
    GD30AD3344_ConfigExternalReference();
    GD30AD3344_Init();

    bsp_adc_init();

    bsp_dac_init();

    bsp_rtc_init();

    key_app_init();

    OLED_Init();

    ota_reset_state();
    frame_parser_init();
    cmd_handler_init(TEAM_DEVICE_ID);
    scheduler_init();
    while (1)
    {
        scheduler_run();
    }
}

/************************************************************
 * Comment  :       重定向 printf 到 RS485 串口
 * Parameter:       ch - 字符
 *                  f  - 文件指针（未使用）
 * Return   :       写入的字符
************************************************************/
int fputc(int ch, FILE *f)
{
    usart_data_transmit(rs485_uart, (uint8_t)ch);
    while (RESET == usart_flag_get(rs485_uart, USART_FLAG_TBE));
    uint32_t tc_timeout = 0xFFFFFUL;
    while ((RESET == usart_flag_get(rs485_uart, USART_FLAG_TC)) && (--tc_timeout > 0U));
    return ch;
}
