// 头文件
#include "cimc_f470vet6.h"

// 函数定义

//
// Comment  :       RTC 任务：刷新 RTC 时间结构体
// Parameter:       none
// Return   :       none
//
void rtc_task(void)
{
    rtc_current_time_get(&rtc_initpara);
}
