//
//    \file    systick.c
//    \brief   the systick configuration file
//
//    \version 2024-12-20, V3.3.1, firmware for GD32F4xx
//
//
//    Copyright (c) 2024, GigaDevice Semiconductor Inc.
//
//    Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//    1. Redistributions of source code must retain the above copyright notice, this
//       list of conditions and the following disclaimer.
//    2. Redistributions in binary form must reproduce the above copyright notice,
//       this list of conditions and the following disclaimer in the documentation
//       and/or other materials provided with the distribution.
//    3. Neither the name of the copyright holder nor the names of its contributors
//       may be used to endorse or promote products derived from this software without
//       specific prior written permission.
//
//    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
// OF SUCH DAMAGE.
//

// 头文件
#include "gd32f4xx.h"
#include "systick.h"

// 变量定义
volatile static uint32_t delay;

// 函数定义

//
// Comment  :       配置 SysTick 定时器（1000Hz 中断）
// Parameter:       none
// Return   :       none
//
void systick_config(void)
{
    if (SysTick_Config(SystemCoreClock / 1000U))
    {
        while (1)
        {
        }
    }
    NVIC_SetPriority(SysTick_IRQn, 0x00U);
}

//
// Comment  :       毫秒级阻塞延时
// Parameter:       count - 延时毫秒数
// Return   :       none
//
void delay_1ms(uint32_t count)
{
    delay = count;

    while (0U != delay)
    {
    }
}

//
// Comment  :       延时计数器递减（SysTick 中断中调用）
// Parameter:       none
// Return   :       none
//
void delay_decrement(void)
{
    if (0U != delay)
    {
        delay--;
    }
}
