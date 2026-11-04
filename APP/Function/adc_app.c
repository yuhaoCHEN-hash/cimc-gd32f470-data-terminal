// 头文件
#include "cimc_f470vet6.h"

// 变量声明
extern uint16_t adc_value[2];

// 变量定义
uint16_t g_potentiometer_mv = 0;
uint16_t g_vref_mv          = 0;

// 函数定义

//
// Comment  :       ADC 原始值转换为毫伏
// Parameter:       raw - ADC 原始采样值（12 位）
// Return   :       对应的毫伏值（0~3300）
//
static uint16_t adc_raw_to_mv(uint16_t raw)
{
    return (uint32_t)raw * 3300 / 4096;
}

//
// Comment  :       获取电位计电压（毫伏）
// Parameter:       none
// Return   :       电位计电压值（mV）
//
uint16_t adc_get_potentiometer_mv(void)
{
    return g_potentiometer_mv;
}

//
// Comment  :       获取参考电压（毫伏）
// Parameter:       none
// Return   :       参考电压值（mV）
//
uint16_t adc_get_vref_mv(void)
{
    return g_vref_mv;
}

//
// Comment  :       获取电位计电压（伏特）
// Parameter:       none
// Return   :       电位计电压值（V）
//
float adc_get_potentiometer_v(void)
{
    return g_potentiometer_mv / 1000.0f;
}

//
// Comment  :       ADC 采样任务：更新电位计和参考电压值
// Parameter:       none
// Return   :       none
//
void adc_task(void)
{
    g_potentiometer_mv = adc_raw_to_mv(adc_value[0]);
    g_vref_mv          = adc_raw_to_mv(adc_value[1]);
}
