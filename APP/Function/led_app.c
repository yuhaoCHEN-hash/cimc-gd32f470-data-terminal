// 头文件
#include "cimc_f470vet6.h"
#include "cmd_handler.h"

// kept for any extern references that may exist elsewhere
uint8_t ucLed[6];
uint32_t led1_tick = 0;

// stub – no longer used internally
void led_disp(uint8_t *buf)
{
    (void)buf;
}

// 函数定义

/************************************************************
 * Comment  :       LED 任务（1ms 周期）：心跳灯和自动上报指示
 * Parameter:       none
 * Return   :       none
************************************************************/
void led_task(void)
{
    uint32_t now = get_system_ms();

    // LED1: system heartbeat 1 Hz (500 ms on / 500 ms off)
    if (now - led1_tick >= 500U)
    {
        LED1_TOGGLE;
        led1_tick = now;
    }

    // LED2: lit while auto-report is active
    if (g_auto_report_active)
    {
        LED2_ON;
    }
    else
    {
        LED2_OFF;
    }

    // LED3-6: not used by this application; force off to prevent
    // stale GPIO state from old key_app or power-glitch toggles
    LED3_OFF;
    LED4_OFF;
    LED5_OFF;
    LED6_OFF;
}
