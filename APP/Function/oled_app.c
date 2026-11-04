// 头文件
#include "cimc_f470vet6.h"
#include "adc_app.h"
#include "dac_app.h"
#include "pt100_app.h"
#include "cmd_handler.h"

// 变量声明
extern rtc_parameter_struct rtc_initpara;

// 变量定义
uint8_t oled_disp_mode = 0;

extern uint8_t s_oled_available;

// 函数定义

//
// Comment  :       OLED 格式化打印
// Parameter:       x      - X 坐标
//                  y      - Y 坐标
//                  format - 格式字符串
//                  ...    - 可变参数
// Return   :       写入的字符数
//
int oled_printf(uint8_t x, uint8_t y, const char *format, ...)
{
    char buffer[64];
    va_list arg;
    int len;

    va_start(arg, format);
    len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    OLED_ShowStr(x, y, buffer, 8);
    return len;
}

//
// Comment  :       OLED 显示任务：显示队号、自动采样状态及 PT100 调试信息
// Parameter:       none
// Return   :       none
//
void oled_task(void)
{
    if (!s_oled_available)
    {
        OLED_Init();
    }

    OLED_ShowStr(0, 0, "2026570827", 16);
    if (cmd_handler_is_auto_reporting())
    {
        OLED_ShowStr(0, 2, "AutoSample", 16);
    }
    else
    {
        OLED_ShowStr(0, 2, "IDLE      ", 16);
    }
}
