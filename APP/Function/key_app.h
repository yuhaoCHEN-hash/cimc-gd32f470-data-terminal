// 头文件
#ifndef KEY_APP_H
#define KEY_APP_H

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

// 宏定义
#define KEY1_PRESSED  1
#define KEY2_PRESSED  2
#define KEY3_PRESSED  3
#define KEY4_PRESSED  4
#define KEY5_PRESSED  5
#define KEY6_PRESSED  6

// 函数声明
void key_app_init(void);
void key_task(void);

#ifdef __cplusplus
}
#endif

#endif
