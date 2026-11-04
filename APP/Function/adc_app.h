// 头文件
#ifndef __ADC_APP_H_
#define __ADC_APP_H_

#include "stdint.h"

// 变量声明
extern uint16_t g_potentiometer_mv;
extern uint16_t g_vref_mv;

// 函数声明
uint16_t adc_get_potentiometer_mv(void);
uint16_t adc_get_vref_mv(void);
float    adc_get_potentiometer_v(void);

void adc_task(void);

#endif // __ADC_APP_H_
