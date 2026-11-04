// 头文件
#ifndef __USART_APP_H__
#define __USART_APP_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 变量声明
extern volatile uint8_t rs485_rx_flag;
extern uint8_t rs485_rx_buffer[];

extern volatile uint8_t usart0_rx_flag;
extern uint8_t usart0_cmd_buffer[256];

// 函数声明
int  my_printf(uint32_t usart_periph, const char *format, ...);
void ota_reset_state(void);
void usart0_task(void);
void uart_task(void);
void rs485_task(void);
void rs485_send_string(const char *str);

#ifdef __cplusplus
}
#endif

#endif
