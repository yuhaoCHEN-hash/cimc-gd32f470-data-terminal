#ifndef __USART_APP_H__
#define __USART_APP_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int  my_printf(uint32_t usart_periph, const char *format, ...);
void uart_task(void);
void rs485_send_frame_pub(const char *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif
