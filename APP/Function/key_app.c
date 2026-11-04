// 头文件
#include "cimc_f470vet6.h"
#include "dac_app.h"

// 变量声明
extern rtc_parameter_struct rtc_initpara;

// 变量定义
static uint8_t key_val = 0;
static uint8_t key_old = 0;
static uint8_t key_down = 0;
static uint8_t key_up   = 0;
static uint32_t press_tick = 0;
static uint8_t  long_press_handled = 0;
static uint32_t last_activity = 0;
static uint8_t time_index = 0;
static uint8_t g_sleep_mode = 0;

// 函数定义

//
// Comment  :       读取所有按键状态
// Parameter:       none
// Return   :       按键值（0 表示无按键按下）
//
static uint8_t key_read(void)
{
    if (KEY1_READ == 0) return KEY1_PRESSED;
    if (KEY2_READ == 0) return KEY2_PRESSED;
    if (KEY3_READ == 0) return KEY3_PRESSED;
    if (KEY4_READ == 0) return KEY4_PRESSED;
    if (KEY5_READ == 0) return KEY5_PRESSED;
    if (KEY6_READ == 0) return KEY6_PRESSED;
    return 0;
}

//
// Comment  :       按键应用初始化：恢复上次睡眠模式
// Parameter:       none
// Return   :       none
//
void key_app_init(void)
{
    g_sleep_mode = RTC_BKP1;
    if (g_sleep_mode > 2) g_sleep_mode = 0;
}

//
// Comment  :       BCD 码增量，避免非法值
// Parameter:       val - 当前 BCD 值
//                  max - 最大值（BCD）
// Return   :       增加后的 BCD 值
//
static uint8_t bcd_inc(uint8_t val, uint8_t max)
{
    val++;
    if ((val & 0x0F) > 0x09) val += 0x06;
    if (val >= max) val = 0;
    return val;
}

//
// Comment  :       BCD 码减量，避免非法值
// Parameter:       val - 当前 BCD 值
//                  max - 最大值（BCD）
// Return   :       减少后的 BCD 值
//
static uint8_t bcd_dec(uint8_t val, uint8_t max)
{
    if (val == 0) val = max;
    else val--;
    if ((val & 0x0F) > 0x09) val -= 0x06;
    return val;
}

//
// Comment  :       只写 RTC 时间寄存器（不改预分频）
// Parameter:       hour - 小时（BCD）
//                  min  - 分钟（BCD）
//                  sec  - 秒（BCD）
// Return   :       none
//
static void rtc_set_time_only(uint8_t hour, uint8_t min, uint8_t sec)
{
    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();

    RTC_WPK = RTC_UNLOCK_KEY1;
    RTC_WPK = RTC_UNLOCK_KEY2;

    rtc_init_mode_enter();

    RTC_TIME = TIME_HR(hour) | TIME_MN(min) | TIME_SC(sec);
    RTC_DATE = DATE_YR(rtc_initpara.year) | DATE_DOW(rtc_initpara.day_of_week) |
               DATE_MON(rtc_initpara.month) | DATE_DAY(rtc_initpara.date);

    rtc_init_mode_exit();
    rtc_register_sync_wait();

    RTC_WPK = RTC_LOCK_KEY;
}

static void key6_enter_sleepmode(void)
{
    rcu_periph_clock_enable(RCU_PMU);
    pmu_to_sleepmode(WFI_CMD);
}

//
// Comment  :       Standby 模式 + RTC ALARM 5 秒自动唤醒
// Parameter:       none
// Return   :       none
//
static void bsp_enter_standby_with_rtc(void)
{
    rtc_alarm_struct rtc_alarm;

    rcu_periph_clock_enable(RCU_PMU);

    rtc_initpara.hour   = 0x00;
    rtc_initpara.minute = 0x00;
    rtc_initpara.second = 0x00;
    rtc_set_time_only(rtc_initpara.hour, rtc_initpara.minute, rtc_initpara.second);

    rtc_alarm_disable(RTC_ALARM0);

    rtc_alarm.alarm_mask       = RTC_ALARM_DATE_MASK | RTC_ALARM_HOUR_MASK | RTC_ALARM_MINUTE_MASK;
    rtc_alarm.weekday_or_date  = 0;
    rtc_alarm.alarm_day        = 0x31;
    rtc_alarm.alarm_hour       = 0x00;
    rtc_alarm.alarm_minute     = 0x00;
    rtc_alarm.alarm_second     = 0x05;
    rtc_alarm.am_pm            = RTC_AM;
    rtc_alarm_config(RTC_ALARM0, &rtc_alarm);

    rtc_interrupt_enable(RTC_INT_ALARM0);
    rtc_alarm_enable(RTC_ALARM0);
    rtc_flag_clear(RTC_FLAG_ALRM0);

    RTC_BKP1 = g_sleep_mode;

    pmu_flag_clear(PMU_FLAG_RESET_STANDBY);
    pmu_to_standbymode();
}

//
// Comment  :       按键任务（5ms 周期）：处理按键事件
// Parameter:       none
// Return   :       none
//
void key_task(void)
{
    static uint32_t time_adc_addr = 0x010000;
    static uint8_t buffer[10];
    uint32_t tick = get_system_ms();

    key_val = key_read();
    key_down = key_val & (key_old ^ key_val);
    key_up   = (~key_val) & (key_old ^ key_val);
    key_old = key_val;

    switch (key_down)
    {
        case KEY1_PRESSED:
        {
            oled_disp_mode = (oled_disp_mode + 1) % 4;
            break;
        }
        case KEY2_PRESSED:
        {
            if (oled_disp_mode == 0)
            {
                switch (time_index)
                {
                    case 0:
                        rtc_initpara.hour = bcd_inc(rtc_initpara.hour, 0x24);
                    break;
                    case 1:
                        rtc_initpara.minute = bcd_inc(rtc_initpara.minute, 0x60);
                    break;
                    case 2:
                        rtc_initpara.second = bcd_inc(rtc_initpara.second, 0x60);
                    break;
                    case 3:
                        rtc_initpara.year = bcd_inc(rtc_initpara.year, 0x99);
                    break;
                    case 4:
                        rtc_initpara.month = bcd_inc(rtc_initpara.month, 0x12);
                    break;
                    case 5:
                        rtc_initpara.date = bcd_inc(rtc_initpara.date, 0x31);
                    break;
                }
                rtc_set_time_only(rtc_initpara.hour, rtc_initpara.minute, rtc_initpara.second);
            }
            else if (oled_disp_mode == 1)
            {
                uint16_t mv = dac_get_mv();
                mv = (mv + 10 > 3300) ? 3300 : mv + 10;
                dac_set_mv(mv);
            }
            break;
        }

        case KEY3_PRESSED:
        {
            if (oled_disp_mode == 0)
            {
                switch (time_index)
                {
                    case 0:
                        rtc_initpara.hour = bcd_dec(rtc_initpara.hour, 0x23);
                    break;
                    case 1:
                        rtc_initpara.minute = bcd_dec(rtc_initpara.minute, 0x59);
                    break;
                    case 2:
                        rtc_initpara.second = bcd_dec(rtc_initpara.second, 0x59);
                    break;
                    case 3:
                        rtc_initpara.year = bcd_dec(rtc_initpara.year, 0x99);
                    break;
                    case 4:
                        rtc_initpara.month = bcd_dec(rtc_initpara.month, 0x12);
                    break;
                    case 5:
                        rtc_initpara.date = bcd_dec(rtc_initpara.date, 0x31);
                    break;
                }
                rtc_set_time_only(rtc_initpara.hour, rtc_initpara.minute, rtc_initpara.second);
            }
            if (oled_disp_mode == 1)
            {
                uint16_t mv = dac_get_mv();
                mv = (mv >= 10) ? mv - 10 : 0;
                dac_set_mv(mv);
            }
            break;
        }
        case KEY4_PRESSED:
        {
            time_index = (time_index + 1) % 6;
            break;
        }
        case KEY5_PRESSED:
        {
            my_printf(rs485_uart, "DAC: PA4=%umV\r\n", dac_get_mv());
            break;
        }
        case KEY6_PRESSED:
        {
            g_sleep_mode = (g_sleep_mode + 1) % 3;
            switch (g_sleep_mode)
            {
                case 0:
                    my_printf(rs485_uart, "enter sleep mode, send UART to wakeup\r\n");
                    bsp_enter_sleepmode();
                    my_printf(rs485_uart, "wakeup from sleep mode\r\n");
                break;
                case 1:
                    my_printf(rs485_uart, "enter deep-sleep mode, press PA0(WK_UP) to wakeup\r\n");
                    bsp_enter_deepsleep();
                    my_printf(rs485_uart, "wakeup from deep-sleep mode\r\n");
                break;
                case 2:
                    my_printf(rs485_uart, "enter standby mode, RTC alarm 5s auto wakeup\r\n");
                    bsp_enter_standby_with_rtc();
                break;
            }
            break;
        }
        default:
            break;
    }
}
