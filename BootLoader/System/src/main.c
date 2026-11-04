
#include "CIMC_MCU.h"
#include "bl_main.h"

int main(void)
{
    systick_config();//初始化系统时钟
    bsp_usart_init();//初始化串口
    /* 初始化 I2C0 并显示 Bootloader BL 期间 OLED 显示  */
    bsp_oled_init();
    OLED_Init();
    OLED_ShowStr(0, 2, "Bootloader", 16);

    bl_execute();//进入BL主状态机(不再返回)

    while(1) {
    }
}

/* 重定向 printf 到 USART1(RS485)，PE8 方向控制 */
int fputc(int ch, FILE *f)
{
    RS485_CS_SET(1);
    usart_data_transmit(rs485_uart, (uint8_t)ch);
    while(RESET == usart_flag_get(rs485_uart, USART_FLAG_TBE));
    while(RESET == usart_flag_get(rs485_uart, USART_FLAG_TC));
    RS485_CS_SET(0);
    return ch;
}
