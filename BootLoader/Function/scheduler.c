/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2026/04/29
* Note:
*/
#include "CIMC_MCU.h"

/* Number of scheduler tasks in scheduler_task[]. */
uint8_t task_num;

typedef struct {
    void (*task_func)(void);
    uint32_t rate_ms;
    uint32_t last_run;
} task_t;

/* Static task table: function pointer, run period in ms, and last run time. */
static task_t scheduler_task[] =
{
     {uart_task, 5,    0},
};

/**
 * @brief 从静态任务表初始化任务计数.
 */
void scheduler_init(void)
{
    task_num = sizeof(scheduler_task) / sizeof(task_t);
}

/**
 * @brief 按周期运行到期任务.
 */
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


