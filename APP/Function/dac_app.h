// 头文件
#ifndef __DAC_APP_H_
#define __DAC_APP_H_

#include "stdint.h"

// 函数声明
void     dac_set_mv(uint16_t mv);
uint16_t dac_get_mv(void);
uint16_t dac_get_raw(void);

#endif // __DAC_APP_H_
