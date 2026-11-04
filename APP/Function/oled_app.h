// 头文件
#ifndef __OLED_APP_H__
#define __OLED_APP_H__

#include "stdint.h"

// 宏定义
#ifdef __cplusplus
extern "C"
{
#endif

// 变量声明
extern uint8_t oled_disp_mode;

// 函数声明
int oled_printf(uint8_t x, uint8_t y, const char *format, ...);
void oled_task(void);

#ifdef __cplusplus
}
#endif

#endif
