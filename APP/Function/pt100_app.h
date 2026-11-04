// 头文件
#ifndef __PT100_APP_H_
#define __PT100_APP_H_

// 变量声明
extern float g_pt100_resistance;
extern float g_pt100_temperature;
extern float g_pt100_raw_voltage;

// 函数声明
float    pt100_get_temperature(void);
float    pt100_get_resistance(void);
void     pt100_task(void);

#endif // __PT100_APP_H_
