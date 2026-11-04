// 头文件
#ifndef OTA_UART_H
#define OTA_UART_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 函数声明
void ota_uart_reset_state(void);
void ota_uart_process_frame(const uint8_t *data, uint32_t len);
void ota_uart_task(void);
bool ota_uart_is_active(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_UART_H */

