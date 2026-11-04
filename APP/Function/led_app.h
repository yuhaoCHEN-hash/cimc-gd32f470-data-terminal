#ifndef __LED_APP_H__
#define __LED_APP_H__

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

// 函数声明
extern volatile uint8_t g_led_app_run;  // 模拟题：RUN_LED_APP 触发后置1

void led_task(void);

#ifdef __cplusplus
}
#endif

#endif
