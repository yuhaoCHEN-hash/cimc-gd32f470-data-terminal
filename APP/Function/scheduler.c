// 头文件
#include "cimc_f470vet6.h"

// 变量定义
uint8_t task_num;

typedef struct
{
    void (*task_func)(void);
    uint32_t rate_ms;
    uint32_t last_run;
} task_t;

static task_t scheduler_task[] =
{
     {led_task,          1,    0}
    ,{key_task,          5,    0}
    ,{adc_task,          20,   0}
    ,{pt100_task,        100,  0}
    ,{oled_task,         10,   0}
    ,{uart_task,         5,    0}
    ,{rs485_task,        100,  0}
    ,{rtc_task,          500,  0}
    ,{alarm_flush_task,  200,  0}  // 延迟写告警 Flash，与 uart_task 解耦
};


// 函数定义

//
// Comment  :       调度器初始化：计算任务总数
// Parameter:       none
// Return   :       none
//
void scheduler_init(void)
{
    task_num = sizeof(scheduler_task) / sizeof(task_t);
}

//
// Comment  :       调度器运行：遍历所有任务，到时间则执行
// Parameter:       none
// Return   :       none
//
void scheduler_run(void)
{
    for (uint8_t i = 0; i < task_num; i++)
    {
        uint32_t now_time = get_system_ms();
        if (now_time >= scheduler_task[i].rate_ms + scheduler_task[i].last_run)
        {
            scheduler_task[i].last_run = now_time;
            scheduler_task[i].task_func();
        }
    }
}
