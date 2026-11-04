
#ifndef CIMC_MCU_H
#define CIMC_MCU_H

#include "gd32f4xx.h"
#include "gd32f4xx_dma.h"
#include "systick.h"
#include "usart_app.h"
#include "oled.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************************************************/
/* Wakeup key */
#define WK_UP_PORT                GPIOA
#define WK_UP_PIN                 GPIO_PIN_0
#define WK_UP_CLK_PORT            RCU_GPIOA

/***************************************************************************************************************/
/* V2 USART1 pin map (PD5 TX, PD6 RX) */
#define USART1_RX_PORT            GPIOD
#define USART1_RX_PIN             GPIO_PIN_6
#define USART1_TX_PORT            GPIOD
#define USART1_TX_PIN             GPIO_PIN_5

#define AF_USART1                 GPIO_AF_7

/***************************************************************************************************************/
/* RS485 direction control (PE8) */
#define RS485_CS_PORT             GPIOE
#define RS485_CS_PORT_RCU         RCU_GPIOE
#define RS485_CS_PIN              GPIO_PIN_8

#define RS485_CS_SET(x)           do { if(x) GPIO_BOP(RS485_CS_PORT) = RS485_CS_PIN; \
                                       else GPIO_BC(RS485_CS_PORT) = RS485_CS_PIN; } while(0)

/***************************************************************************************************************/
/* rs485_uart = USART1 (PD5/PD6, RS485) — 唯一与电脑连接的串口
 * PE8 控制 RS485 方向: 1=发送, 0=接收 */
#define rs485_uart               (USART1)
#define RS232_RS485_USART         (USART1)

#define USART1_RDATA_ADDRESS      ((uint32_t)&USART_DATA(USART1))
#define USART0_RDATA_ADDRESS      ((uint32_t)&USART_DATA(USART0))

#define USART1_RX_DMA_PERIPH      DMA0
#define USART1_RX_DMA_CHANNEL     DMA_CH5
#define USART1_RX_DMA_SUBPERI     DMA_SUBPERI4

/* OTA DMA alias used by bootloader & app protocol */
#define OTA_UART_DMA              USART1_RX_DMA_PERIPH
#define OTA_UART_DMA_RCU          RCU_DMA0
#define OTA_UART_DMA_CH           USART1_RX_DMA_CHANNEL
#define OTA_UART_DMA_SUBPERI      USART1_RX_DMA_SUBPERI
#define OTA_UART_RDATA_ADDRESS    USART1_RDATA_ADDRESS
#define OTA_UART_RXBUF_SIZE       4096U

/* Compatibility aliases */
#define USART0_RX_DMA_PERIPH      DMA1
#define USART0_RX_DMA_CHANNEL     DMA_CH5
#define USART0_RX_DMA_SUBPERI     DMA_SUBPERI4

#define USART_PORT                USART1_TX_PORT
#define USARTI_CLK_PORT           RCU_GPIOD
#define USART_TX                  USART1_TX_PIN
#define USART_RX                  USART1_RX_PIN

/***************************************************************************************************************/
/* OLED (I2C0: PB8 SCL, PB9 SDA) */
#define OLED_PORT                 GPIOB
#define OLED_CLK_PORT             RCU_GPIOB
#define OLED_DAT_PIN              GPIO_PIN_9
#define OLED_CLK_PIN              GPIO_PIN_8
#define AF_OLED_I2C0              GPIO_AF_4
#define I2C0_OWN_ADDRESS7         0x72
#define I2C0_DATA_ADDRESS         ((uint32_t)&I2C_DATA(I2C0))

void bsp_usart1_init(void);
void bsp_usart_init(void);
void bsp_wkup_key_exti_init(void);
void bsp_enter_deepsleep(void);
void bsp_oled_init(void);
void oled_show_bootloader(void);



#ifdef __cplusplus
  }
#endif

#endif /* CIMC_MCU_H */
