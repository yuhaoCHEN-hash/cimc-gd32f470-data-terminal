// 头文件
#include <math.h>
#include "cimc_f470vet6.h"

// 变量定义
float g_pt100_resistance  = 0.0f;
float g_pt100_temperature = 0.0f;
float g_pt100_raw_voltage = 0.0f;

//
// 模拟信号链路:
//   恒流激励(1mA) → RTD → 仪表放大(×10) → 差分级(增益3.16, 偏置0.543V) → ADC
//
// 反向推导:
//   V_rtd = (V_adc / 3.16 + 0.543) / 10
//   R     = V_rtd / 0.001
//
static const float FRONT_GAIN   = 3.16f;
static const float FRONT_BIAS_V = 0.543f;
static const float INAMP_GAIN   = 10.0f;
static const float EXCIT_CURR_A = 0.001f;

// 电阻线性校正系数（R_true = (R_meas - b) / a），由标定数据最小二乘拟合得出
#define RESIST_CALIB_A  (0.9172f)
#define RESIST_CALIB_B  (1.785f)

// 二次多项式补偿系数，由标定数据最小二乘拟合得出
#define CALIB_A2  (0.0000135329f)
#define CALIB_A1  (0.992506f)
#define CALIB_A0  (-1.168839f)

// 函数定义

//
// Comment  :       Callendar-Van Dusen 方程逆运算，阻值转温度
// Parameter:       R - PT100 实测电阻值（欧姆）
// Return   :       摄氏温度值，异常返回 -999.0
//
static float cvd_solve_celsius(float R)
{
    const float R0 = 100.0f;
    const float A  = 3.9083e-3f;
    const float B  = -5.775e-7f;

    float dev = (R - R0) / R0;
    float det = A * A + 4.0f * B * dev;

    if (det < 0.0f)
        return -999.0f;

    float T_raw = (-A + sqrtf(det)) / (2.0f * B);
    return CALIB_A2 * T_raw * T_raw + CALIB_A1 * T_raw + CALIB_A0;
}

//
// Comment  :       获取当前 PT100 温度值
// Parameter:       none
// Return   :       摄氏温度
//
float pt100_get_temperature(void)
{
    return g_pt100_temperature;
}

//
// Comment  :       获取当前 PT100 电阻值
// Parameter:       none
// Return   :       电阻值（欧姆）
//
float pt100_get_resistance(void)
{
    return g_pt100_resistance;
}

// 第一级：16 点滑动均值滤波器
#define SMA_WIN 16

static float   s_sma_buf[SMA_WIN];
static float   s_sma_acc = 0.0f;
static uint8_t s_sma_pos = 0;
static uint8_t s_sma_rdy = 0;

//
// Comment  :       SMA 滤波器：向滑动窗口推入新值
// Parameter:       x - 新采样值
// Return   :       滤波后的输出值
//
static float sma_push(float x)
{
    if (!s_sma_rdy && s_sma_pos == 0)
    {
        for (uint8_t i = 0; i < SMA_WIN; i++)
            s_sma_buf[i] = x;
        s_sma_acc = x * SMA_WIN;
        s_sma_pos = 1 % SMA_WIN;
        s_sma_rdy = 1;
        return x;
    }

    s_sma_acc -= s_sma_buf[s_sma_pos];
    s_sma_buf[s_sma_pos] = x;
    s_sma_acc += x;
    s_sma_pos = (s_sma_pos + 1) % SMA_WIN;
    return s_sma_acc / SMA_WIN;
}

// 第二级：IIR 指数平滑滤波器
#define EMA_COEF 0.15f

static float   s_ema_out = 0.0f;
static uint8_t s_ema_rdy = 0;

//
// Comment  :       EMA 滤波器：指数平滑
// Parameter:       x - 新采样值
// Return   :       滤波后的输出值
//
static float ema_push(float x)
{
    if (!s_ema_rdy)
    {
        s_ema_out = x;
        s_ema_rdy = 1;
        return x;
    }
    s_ema_out = EMA_COEF * x + (1.0f - EMA_COEF) * s_ema_out;
    return s_ema_out;
}

//
// Comment  :       PT100 测量任务（100ms 周期）
// Parameter:       none
// Return   :       none
//
void pt100_task(void)
{
    float raw      = GD30AD3344_AD_Read(GD30AD3344_Channel_4, GD30AD3344_PGA_4V096);
    float smoothed = sma_push(raw);
    g_pt100_raw_voltage = ema_push(smoothed);

    g_pt100_resistance  = (g_pt100_raw_voltage / FRONT_GAIN + FRONT_BIAS_V) / (INAMP_GAIN * EXCIT_CURR_A);
    g_pt100_resistance  = (g_pt100_resistance - RESIST_CALIB_B) / RESIST_CALIB_A;
    g_pt100_temperature = cvd_solve_celsius(g_pt100_resistance);
}
