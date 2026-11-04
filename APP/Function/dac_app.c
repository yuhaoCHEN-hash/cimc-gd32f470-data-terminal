// 头文件
#include "cimc_f470vet6.h"

// 变量定义
static uint16_t g_dac_raw = 0;

// 函数定义

//
// Comment  :       设置 DAC 输出电压（毫伏）
// Parameter:       mv - 目标电压值（0~3300 mV）
// Return   :       none
//
void dac_set_mv(uint16_t mv)
{
    if (mv > 3300) mv = 3300;
    g_dac_raw = (uint32_t)mv * 4096 / 3300;
    dac_data_set(DAC0, DAC_OUT0, DAC_ALIGN_12B_R, g_dac_raw);
}

//
// Comment  :       获取 DAC 当前输出电压（毫伏）
// Parameter:       none
// Return   :       当前电压值（mV）
//
uint16_t dac_get_mv(void)
{
    return (uint32_t)g_dac_raw * 3300 / 4096;
}

//
// Comment  :       获取 DAC 原始寄存器值
// Parameter:       none
// Return   :       DAC 原始值（12 位）
//
uint16_t dac_get_raw(void)
{
    return g_dac_raw;
}
