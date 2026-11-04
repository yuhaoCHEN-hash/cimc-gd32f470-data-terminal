// 头文件
#ifndef CIMC_F470VET6_H
#define CIMC_F470VET6_H

#include "gd32f4xx.h"
#include "gd32f4xx_dma.h"
#include "systick.h"

#include "key_app.h"
#include "oled.h"
#include "ota_uart.h"
#include "gd25qxx.h"
#include "gd30ad3344.h"
#include "led_app.h"
#include "adc_app.h"
#include "dac_app.h"
#include "pt100_app.h"
#include "oled_app.h"
#include "usart_app.h"
#include "rtc_app.h"
#include "alarm_app.h"

#include "scheduler.h"

#include "perf_counter.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// 宏定义：V2 pin map
// =============================================================================================================
// ADC
#define ADC_IN10_PORT        GPIOC
#define ADC_IN10_PIN         GPIO_PIN_0
#define ADC_IN12_PORT        GPIOC
#define ADC_IN12_PIN         GPIO_PIN_2

// =============================================================================================================
// ETH
#define ETH_MDC_PORT         GPIOC
#define ETH_MDC_PIN          GPIO_PIN_1
#define ETH_REF_CLK_PORT     GPIOA
#define ETH_REF_CLK_PIN      GPIO_PIN_1
#define ETH_MDIO_PORT        GPIOA
#define ETH_MDIO_PIN         GPIO_PIN_2
#define ETH_CRS_DV_PORT      GPIOA
#define ETH_CRS_DV_PIN       GPIO_PIN_7
#define ETH_RXD0_PORT        GPIOC
#define ETH_RXD0_PIN         GPIO_PIN_4
#define ETH_RXD1_PORT        GPIOC
#define ETH_RXD1_PIN         GPIO_PIN_5
#define ETH_TX_EN_PORT       GPIOB
#define ETH_TX_EN_PIN        GPIO_PIN_11
#define ETH_TXD0_PORT        GPIOB
#define ETH_TXD0_PIN         GPIO_PIN_12
#define ETH_TXD1_PORT        GPIOB
#define ETH_TXD1_PIN         GPIO_PIN_13

// =============================================================================================================
// Control
#define WK_UP_PORT           GPIOA
#define WK_UP_PIN            GPIO_PIN_0
#define W_RST_PORT           GPIOA
#define W_RST_PIN            GPIO_PIN_3
#define PHY_RST_PORT         GPIOD
#define PHY_RST_PIN          GPIO_PIN_3

// =============================================================================================================
// DAC
#define DAC0_OUT_PORT        GPIOA
#define DAC0_OUT_PIN         GPIO_PIN_4
#define DAC1_OUT_PORT        GPIOA
#define DAC1_OUT_PIN         GPIO_PIN_5

// =============================================================================================================
// CS/GPIO
#define RS485_CS_PORT        GPIOE
#define RS485_CS_PORT_RCU    RCU_GPIOE
#define RS485_CS_PIN         GPIO_PIN_8
#define W_SPI_CS_PORT        GPIOE
#define W_SPI_CS_PIN         GPIO_PIN_9
#define S_SPI_CS_PORT        GPIOE
#define S_SPI_CS_PIN         GPIO_PIN_10

#define RS485_CS_SET(x)      do { if(x) GPIO_BOP(RS485_CS_PORT) = RS485_CS_PIN; else GPIO_BC(RS485_CS_PORT) = RS485_CS_PIN; } while(0)

#define RS232_RS485_USART         (USART1)
#define USART_RS_RCU              RCU_USART1
#define USART_RS_PORT             USART1_TX_PORT
#define USART_RS_PORT_RCU         RCU_GPIOD
#define USART_RS_RX_PORT          USART1_RX_PORT
#define USART_RS_RX_PIN           USART1_RX_PIN
#define USART_RS_TX_PORT          USART1_TX_PORT
#define USART_RS_TX_PIN           USART1_TX_PIN
#define USART_RS_RX               USART_RS_RX_PIN
#define USART_RS_TX               USART_RS_TX_PIN
#define USART_RS_RDATA_ADDRESS    USART1_RDATA_ADDRESS

#define DMA_RS_USART              USART1_RX_DMA_PERIPH
#define DMA_RS_USART_CHANNEL_RX   USART1_RX_DMA_CHANNEL
#define DMA_RS_USART_SUBPERI_RX   USART1_RX_DMA_SUBPERI

// =============================================================================================================
// SPI3
#define SPI3_SCK_PORT        GPIOE
#define SPI3_SCK_PIN         GPIO_PIN_12
#define SPI3_MISO_PORT       GPIOE
#define SPI3_MISO_PIN        GPIO_PIN_13
#define SPI3_MOSI_PORT       GPIOE
#define SPI3_MOSI_PIN        GPIO_PIN_14

// =============================================================================================================
// OLED (I2C0)
#define OLED_DAT_PORT        GPIOB
#define OLED_DAT_PIN_V2      GPIO_PIN_9
#define OLED_CLK_PORT_V2     GPIOB
#define OLED_CLK_PIN_V2      GPIO_PIN_8

// =============================================================================================================
// SPI FLASH (SPI0)
#define SPI_FLASH_SCK_PORT   GPIOB
#define SPI_FLASH_SCK_PIN    GPIO_PIN_3
#define SPI_FLASH_MISO_PORT  GPIOB
#define SPI_FLASH_MISO_PIN   GPIO_PIN_4
#define SPI_FLASH_MOSI_PORT  GPIOB
#define SPI_FLASH_MOSI_PIN   GPIO_PIN_5
#define SPI_FLASH_CS_PORT    GPIOA
#define SPI_FLASH_CS_PIN     GPIO_PIN_15

// =============================================================================================================
// USART
#define USART1_RX_PORT       GPIOD
#define USART1_RX_PIN        GPIO_PIN_6
#define USART1_TX_PORT       GPIOD
#define USART1_TX_PIN        GPIO_PIN_5
#define USART0_RX_PORT       GPIOA
#define USART0_RX_PIN        GPIO_PIN_10
#define USART0_TX_PORT       GPIOA
#define USART0_TX_PIN        GPIO_PIN_9
#define USART5_RX_PORT       GPIOC
#define USART5_RX_PIN        GPIO_PIN_7
#define USART5_TX_PORT       GPIOC
#define USART5_TX_PIN        GPIO_PIN_6
#define USART2_RX_PORT       GPIOD
#define USART2_RX_PIN        GPIO_PIN_9
#define USART2_TX_PORT       GPIOD
#define USART2_TX_PIN        GPIO_PIN_8

// =============================================================================================================
// AF map
#define AF_OLED_I2C0         GPIO_AF_4
#define AF_SPI_FLASH         GPIO_AF_5
#define AF_SPI3              GPIO_AF_5
#define AF_USART0            GPIO_AF_7
#define AF_USART1            GPIO_AF_7
#define AF_USART2            GPIO_AF_7
#define AF_USART5            GPIO_AF_8
#define AF_ETH               GPIO_AF_11

// =============================================================================================================
// LED IO map
#define LED1_PORT       GPIOB
#define LED1_PIN        GPIO_PIN_12
#define LED2_PORT       GPIOB
#define LED2_PIN        GPIO_PIN_13
#define LED3_PORT       GPIOB
#define LED3_PIN        GPIO_PIN_14
#define LED4_PORT       GPIOB
#define LED4_PIN        GPIO_PIN_15
#define LED5_PORT       GPIOD
#define LED5_PIN        GPIO_PIN_8
#define LED6_PORT       GPIOD
#define LED6_PIN        GPIO_PIN_9

#define LED_ACTIVE_HIGH 1

#if LED_ACTIVE_HIGH
#define LED_WRITE(port, pin, on) do { if(on) GPIO_BOP(port) = (pin); else GPIO_BC(port) = (pin); } while(0)
#else
#define LED_WRITE(port, pin, on) do { if(on) GPIO_BC(port) = (pin); else GPIO_BOP(port) = (pin); } while(0)
#endif

#define LED1_SET(x)     do { LED_WRITE(LED1_PORT, LED1_PIN, (x)); } while(0)
#define LED2_SET(x)     do { LED_WRITE(LED2_PORT, LED2_PIN, (x)); } while(0)
#define LED3_SET(x)     do { LED_WRITE(LED3_PORT, LED3_PIN, (x)); } while(0)
#define LED4_SET(x)     do { LED_WRITE(LED4_PORT, LED4_PIN, (x)); } while(0)
#define LED5_SET(x)     do { LED_WRITE(LED5_PORT, LED5_PIN, (x)); } while(0)
#define LED6_SET(x)     do { LED_WRITE(LED6_PORT, LED6_PIN, (x)); } while(0)

#define LED1_TOGGLE     do { GPIO_TG(LED1_PORT) = LED1_PIN; } while(0)
#define LED2_TOGGLE     do { GPIO_TG(LED2_PORT) = LED2_PIN; } while(0)
#define LED3_TOGGLE     do { GPIO_TG(LED3_PORT) = LED3_PIN; } while(0)
#define LED4_TOGGLE     do { GPIO_TG(LED4_PORT) = LED4_PIN; } while(0)
#define LED5_TOGGLE     do { GPIO_TG(LED5_PORT) = LED5_PIN; } while(0)
#define LED6_TOGGLE     do { GPIO_TG(LED6_PORT) = LED6_PIN; } while(0)

#define LED1_ON         do { LED_WRITE(LED1_PORT, LED1_PIN, 1); } while(0)
#define LED2_ON         do { LED_WRITE(LED2_PORT, LED2_PIN, 1); } while(0)
#define LED3_ON         do { LED_WRITE(LED3_PORT, LED3_PIN, 1); } while(0)
#define LED4_ON         do { LED_WRITE(LED4_PORT, LED4_PIN, 1); } while(0)
#define LED5_ON         do { LED_WRITE(LED5_PORT, LED5_PIN, 1); } while(0)
#define LED6_ON         do { LED_WRITE(LED6_PORT, LED6_PIN, 1); } while(0)

#define LED1_OFF        do { LED_WRITE(LED1_PORT, LED1_PIN, 0); } while(0)
#define LED2_OFF        do { LED_WRITE(LED2_PORT, LED2_PIN, 0); } while(0)
#define LED3_OFF        do { LED_WRITE(LED3_PORT, LED3_PIN, 0); } while(0)
#define LED4_OFF        do { LED_WRITE(LED4_PORT, LED4_PIN, 0); } while(0)
#define LED5_OFF        do { LED_WRITE(LED5_PORT, LED5_PIN, 0); } while(0)
#define LED6_OFF        do { LED_WRITE(LED6_PORT, LED6_PIN, 0); } while(0)

// 函数声明
void bsp_led_init(void);

// =============================================================================================================
// KEY IO map
#define KEYC_PORT        GPIOC
#define KEYE_PORT        GPIOE
#define KEYA_PORT        GPIOA
#define KEYC_CLK_PORT    RCU_GPIOC
#define KEYE_CLK_PORT    RCU_GPIOE
#define KEYA_CLK_PORT    RCU_GPIOA

#define KEY1_PIN        GPIO_PIN_13
#define KEY2_PIN        GPIO_PIN_6
#define KEY3_PIN        GPIO_PIN_5
#define KEY4_PIN        GPIO_PIN_4
#define KEY5_PIN        GPIO_PIN_3
#define KEY6_PIN        GPIO_PIN_2
#define KEYW_PIN        GPIO_PIN_0

#define KEY1_READ       gpio_input_bit_get(KEYC_PORT, KEY1_PIN)
#define KEY2_READ       gpio_input_bit_get(KEYE_PORT, KEY2_PIN)
#define KEY3_READ       gpio_input_bit_get(KEYE_PORT, KEY3_PIN)
#define KEY4_READ       gpio_input_bit_get(KEYE_PORT, KEY4_PIN)
#define KEY5_READ       gpio_input_bit_get(KEYE_PORT, KEY5_PIN)
#define KEY6_READ       gpio_input_bit_get(KEYE_PORT, KEY6_PIN)
#define KEYW_READ       gpio_input_bit_get(KEYA_PORT, KEYW_PIN)

void bsp_btn_init(void);
void bsp_wkup_key_exti_init(void);
void bsp_key1_exti_init(void);
void bsp_enter_sleepmode(void);
void bsp_enter_standby_wkup(void);
void bsp_enter_deepsleep(void);
void bsp_enter_deepsleep_rtc_wakeup(uint16_t seconds);

// OLED
#define I2C0_OWN_ADDRESS7      0x72
#define I2C0_SLAVE_ADDRESS7    0x82
#define I2C0_DATA_ADDRESS      (uint32_t)&I2C_DATA(I2C0)

#define OLED_PORT        OLED_DAT_PORT
#define OLED_CLK_PORT    RCU_GPIOB
#define OLED_DAT_PIN     OLED_DAT_PIN_V2
#define OLED_CLK_PIN     OLED_CLK_PIN_V2

void bsp_oled_init(void);

// gd25qxx
#define SPI_PORT              SPI_FLASH_SCK_PORT
#define SPI_CLK_PORT          RCU_GPIOB
#define SPI_CS_PORT           SPI_FLASH_CS_PORT
#define SPI_CS_CLK_PORT       RCU_GPIOA

#define SPI_NSS               SPI_FLASH_CS_PIN
#define SPI_SCK               SPI_FLASH_SCK_PIN
#define SPI_MISO              SPI_FLASH_MISO_PIN
#define SPI_MOSI              SPI_FLASH_MOSI_PIN

void bsp_gd25qxx_init(void);

// gd30ad3344
#define SPI3_PORT              SPI3_SCK_PORT
#define SPI3_CLK_PORT          RCU_GPIOE

#define SPI3_NSS               W_SPI_CS_PIN
#define SPI3_SCK               SPI3_SCK_PIN
#define SPI3_MISO              SPI3_MISO_PIN
#define SPI3_MOSI              SPI3_MOSI_PIN

#define GD30_SPIMODE           SPI_CK_PL_LOW_PH_2EDGE
#define GD30_SPI_PRESCALE      SPI_PSC_32

#define GD30_SPI               SPI3
#define GD30_DMA               DMA1
#define GD30_DMA_CHANNEL_TX    DMA_CH1
#define GD30_DMA_CHANNEL_RX    DMA_CH0
#define GD30_DMA_SUBPERI       DMA_SUBPERI4

#define GD30_DMA_RCU           RCU_DMA1
#define GD30_SPI_RCU           RCU_SPI3

#define GD30_SPI_PORT          SPI3_PORT
#define GD30_SPI_PORT_RCU      SPI3_CLK_PORT
#define GD30_SPI_SCK           SPI3_SCK
#define GD30_SPI_MISO          SPI3_MISO
#define GD30_SPI_MOSI          SPI3_MOSI

#define GD30_CS_PORT           W_SPI_CS_PORT
#define GD30_CS_PORT_RCU       RCU_GPIOE
#define GD30_CS_PIN            W_SPI_CS_PIN

#define GD30_CS_LOW()          gpio_bit_reset(GD30_CS_PORT, GD30_CS_PIN)
#define GD30_CS_HIGH()         gpio_bit_set  (GD30_CS_PORT, GD30_CS_PIN)

void bsp_gd30ad3344_init(void);

// USART
#define BSP_USART5_ENABLE         0U

#define rs485_uart               (USART1)
#define USART0_RDATA_ADDRESS      ((uint32_t)&USART_DATA(USART0))
#define USART1_RDATA_ADDRESS      ((uint32_t)&USART_DATA(USART1))
#define USART2_RDATA_ADDRESS      ((uint32_t)&USART_DATA(USART2))
#define USART5_RDATA_ADDRESS      ((uint32_t)&USART_DATA(USART5))

#define USART0_RX_DMA_PERIPH      DMA1
#define USART0_RX_DMA_CHANNEL     DMA_CH5
#define USART0_RX_DMA_SUBPERI     DMA_SUBPERI4

#define USART1_RX_DMA_PERIPH      DMA0
#define USART1_RX_DMA_CHANNEL     DMA_CH5
#define USART1_RX_DMA_SUBPERI     DMA_SUBPERI4

#define USART2_RX_DMA_PERIPH      DMA0
#define USART2_RX_DMA_CHANNEL     DMA_CH1
#define USART2_RX_DMA_SUBPERI     DMA_SUBPERI4

#define USART5_RX_DMA_PERIPH      DMA1
#define USART5_RX_DMA_CHANNEL     DMA_CH1
#define USART5_RX_DMA_SUBPERI     DMA_SUBPERI5

#define USART0_PORT               USART0_TX_PORT
#define USART0_CLK_PORT           RCU_GPIOA

#define USART0_TX                 USART0_TX_PIN
#define USART0_RX                 USART0_RX_PIN

#define RS485_RXBUF_SIZE          256U

#define OTA_UART_PERIPH           USART1
#define OTA_UART_RCU              RCU_USART1
#define OTA_UART_IRQN             USART1_IRQn
#define OTA_UART_PORT             USART1_TX_PORT
#define OTA_UART_PORT_RCU         RCU_GPIOD
#define OTA_UART_TX_PIN           USART1_TX_PIN
#define OTA_UART_RX_PIN           USART1_RX_PIN
#define OTA_UART_AF               AF_USART1
#define OTA_UART_DMA              USART1_RX_DMA_PERIPH
#define OTA_UART_DMA_RCU          RCU_DMA0
#define OTA_UART_DMA_CH           USART1_RX_DMA_CHANNEL
#define OTA_UART_DMA_SUBPERI      USART1_RX_DMA_SUBPERI
#define OTA_UART_BAUDRATE         19200U
#define OTA_UART_RXBUF_SIZE       4096U
#define OTA_UART_RDATA_ADDRESS    USART1_RDATA_ADDRESS

void bsp_usart0_init(void);
void bsp_usart_init(void);
void bsp_usart1_ota_init(void);
void bsp_usart1_ota_init_with_baud(uint32_t baudrate);
void bsp_usart1_init(void);
void bsp_usart2_init(void);
void bsp_usart5_init(void);
void bsp_usart_all_init(void);
void bsp_ota_uart_dma_rearm(void);
uint32_t bsp_ota_uart_dma_received_len(void);

// ADC
#define ADC1_PORT       ADC_IN10_PORT
#define ADC1_CLK_PORT   RCU_GPIOC

#define ADC1_PIN        ADC_IN10_PIN
#define ADC_VREF_PIN    ADC_IN12_PIN

#define ADC_DMA         DMA1
#define ADC_DMA_RCU     RCU_DMA1
#define ADC_DMA_CHANNEL DMA_CH4
#define ADC_DMA_SUBPERI DMA_SUBPERI0

void bsp_adc_init(void);

// DAC
#define CONVERT_NUM                     (1)
#define DAC0_R12DH_ADDRESS              (0x40007408)

#define DAC1_PORT       DAC0_OUT_PORT
#define DAC1_CLK_PORT   RCU_GPIOA

#define DAC1_PIN        DAC0_OUT_PIN

void bsp_dac_init(void);

// RTC
#define RTC_CLOCK_SOURCE_LXTAL
#define BKP_VALUE    0x32F0

extern rtc_parameter_struct rtc_initpara;

int bsp_rtc_init(void);

#ifdef __cplusplus
  }
#endif

#endif // CIMC_F470VET6_H
