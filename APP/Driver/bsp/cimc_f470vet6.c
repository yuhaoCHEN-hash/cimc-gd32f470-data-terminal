// 头文件
#include "cimc_f470vet6.h"

// 变量定义
__IO uint8_t oled_cmd_buf[2] = {0x00, 0x00};
__IO uint8_t oled_data_buf[2] = {0x40, 0x00};

uint8_t spi3_send_array[ARRAYSIZE] = {0};
uint8_t spi3_receive_array[ARRAYSIZE] = {0};

uint8_t spi1_send_array[ARRAYSIZE] = {0};
uint8_t spi1_receive_array[ARRAYSIZE] = {0};

uint8_t usart0_rxbuffer[512];
uint8_t rs485_rxbuffer[RS485_RXBUF_SIZE];
uint8_t usart2_rxbuffer[256];
uint8_t usart5_rxbuffer[256];

uint8_t rxbuffer[OTA_UART_RXBUF_SIZE];

uint16_t adc_value[2];

uint16_t convertarr[CONVERT_NUM] = {0};

rtc_parameter_struct rtc_initpara;
rtc_alarm_struct  rtc_alarm;
__IO uint32_t prescaler_a = 0, prescaler_s = 0;
uint32_t RTCSRC_FLAG = 0;

// 函数定义

/************************************************************
 * Comment  :       初始化 LED GPIO（PB12~PB15, PD8~PD9）
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_led_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOD);

    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);

    gpio_mode_set(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GPIO_PIN_8 | GPIO_PIN_9);
    gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_8 | GPIO_PIN_9);

    LED1_OFF;
    LED2_OFF;
    LED3_OFF;
    LED4_OFF;
    LED5_OFF;
    LED6_OFF;
}

/************************************************************
 * Comment  :       初始化按键 GPIO（PC13, PE2~PE6, PA0）
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_btn_init(void)
{
    rcu_periph_clock_enable(KEYC_CLK_PORT);
    rcu_periph_clock_enable(KEYE_CLK_PORT);
    rcu_periph_clock_enable(KEYA_CLK_PORT);

    gpio_mode_set(KEYC_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, KEY1_PIN);
    gpio_mode_set(KEYE_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, KEY2_PIN | KEY3_PIN | KEY4_PIN | KEY5_PIN | KEY6_PIN);
    gpio_mode_set(KEYA_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, KEYW_PIN);
}

/************************************************************
 * Comment  :       初始化 WKUP 按键外部中断（PA0，双边沿）
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_wkup_key_exti_init(void)
{
    rcu_periph_clock_enable(KEYA_CLK_PORT);
    rcu_periph_clock_enable(RCU_SYSCFG);

    gpio_mode_set(KEYA_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, KEYW_PIN);

    syscfg_exti_line_config(EXTI_SOURCE_GPIOA, EXTI_SOURCE_PIN0);
    exti_init(EXTI_0, EXTI_INTERRUPT, EXTI_TRIG_BOTH);
    exti_interrupt_flag_clear(EXTI_0);
    nvic_irq_enable(EXTI0_IRQn, 1U, 0U);
}

/************************************************************
 * Comment  :       深度睡眠前关闭 USART（停止 IDLE 中断和 DMA）
 * Parameter:       none
 * Return   :       none
************************************************************/
static void bsp_usart_disable_for_deepsleep(void)
{
    usart_interrupt_disable(USART0, USART_INT_IDLE);
    nvic_irq_disable(USART0_IRQn);
    usart_dma_receive_config(USART0, USART_RECEIVE_DMA_DISABLE);
    dma_channel_disable(DMA1, DMA_CH5);
    usart_disable(USART0);

    usart_interrupt_disable(USART1, USART_INT_IDLE);
    nvic_irq_disable(USART1_IRQn);
    usart_dma_receive_config(USART1, USART_RECEIVE_DMA_DISABLE);
    dma_channel_disable(DMA0, DMA_CH5);
    usart_disable(USART1);

    usart_interrupt_disable(USART2, USART_INT_IDLE);
    nvic_irq_disable(USART2_IRQn);
    usart_dma_receive_config(USART2, USART_RECEIVE_DMA_DISABLE);
    dma_channel_disable(USART2_RX_DMA_PERIPH, USART2_RX_DMA_CHANNEL);
    usart_disable(USART2);

    usart_interrupt_disable(USART5, USART_INT_IDLE);
    nvic_irq_disable(USART5_IRQn);
    usart_dma_receive_config(USART5, USART_RECEIVE_DMA_DISABLE);
    dma_channel_disable(USART5_RX_DMA_PERIPH, USART5_RX_DMA_CHANNEL);
    usart_disable(USART5);
}

/************************************************************
 * Comment  :       深度睡眠前关闭 OLED
 * Parameter:       none
 * Return   :       none
************************************************************/
static void bsp_oled_disable_for_deepsleep(void)
{
    OLED_Display_Off();

    i2c_dma_config(I2C0, I2C_DMA_OFF);
    dma_channel_disable(DMA0, DMA_CH6);
    i2c_disable(I2C0);

    gpio_mode_set(OLED_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, OLED_DAT_PIN | OLED_CLK_PIN);
}

/************************************************************
 * Comment  :       深度睡眠前关闭 SPI
 * Parameter:       none
 * Return   :       none
************************************************************/
static void bsp_spi_disable_for_deepsleep(void)
{
    SPI_FLASH_CS_HIGH();
    GD30_CS_HIGH();

    spi_dma_disable(SPI0, SPI_DMA_RECEIVE);
    spi_dma_disable(SPI0, SPI_DMA_TRANSMIT);
    spi_dma_disable(SPI3, SPI_DMA_RECEIVE);
    spi_dma_disable(SPI3, SPI_DMA_TRANSMIT);

    dma_channel_disable(DMA1, DMA_CH2);
    dma_channel_disable(DMA1, DMA_CH3);
    dma_channel_disable(DMA1, DMA_CH4);

    spi_disable(SPI0);
    spi_disable(SPI3);
}

/************************************************************
 * Comment  :       深度睡眠前将 GPIO 置为模拟/低功耗状态
 * Parameter:       none
 * Return   :       none
************************************************************/
static void bsp_gpio_enter_deepsleep_state(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOE);

    LED1_OFF;
    LED2_OFF;
    LED3_OFF;
    LED4_OFF;
    LED5_OFF;
    LED6_OFF;

    gpio_mode_set(KEYC_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, KEY1_PIN);
    gpio_mode_set(KEYE_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  KEY2_PIN | KEY3_PIN | KEY4_PIN | KEY5_PIN | KEY6_PIN);

    gpio_mode_set(USART0_TX_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, USART0_TX_PIN);
    gpio_mode_set(USART0_RX_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, USART0_RX_PIN);
    gpio_mode_set(USART1_TX_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, USART1_TX_PIN);
    gpio_mode_set(USART1_RX_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, USART1_RX_PIN);
    gpio_mode_set(USART2_TX_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, USART2_TX_PIN);
    gpio_mode_set(USART2_RX_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, USART2_RX_PIN);
    gpio_mode_set(USART5_TX_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, USART5_TX_PIN);
    gpio_mode_set(USART5_RX_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, USART5_RX_PIN);

    gpio_mode_set(OLED_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  OLED_DAT_PIN | OLED_CLK_PIN);

    gpio_mode_set(SPI_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  SPI_SCK | SPI_MISO | SPI_MOSI);
    gpio_mode_set(SPI_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI_NSS);
    gpio_output_options_set(SPI_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, SPI_NSS);
    GPIO_BOP(SPI_CS_PORT) = SPI_NSS;

    gpio_mode_set(SPI3_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  SPI3_SCK | SPI3_MISO | SPI3_MOSI);
    gpio_mode_set(SPI3_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI3_NSS);
    gpio_output_options_set(SPI3_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, SPI3_NSS);
    GPIO_BOP(SPI3_PORT) = SPI3_NSS;

    gpio_mode_set(ADC1_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, ADC1_PIN | ADC_VREF_PIN);
    gpio_mode_set(DAC1_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, DAC1_PIN);

    gpio_mode_set(GPIOC, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12);
    gpio_mode_set(GPIOD, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_2);

    gpio_mode_set(KEYA_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, KEYW_PIN);
}

/************************************************************
 * Comment  :       深度睡眠唤醒后重新初始化外设
 * Parameter:       none
 * Return   :       none
************************************************************/
static void bsp_deepsleep_reinit_after_wakeup(void)
{
    extern void system_clock_config(void);
    system_clock_config();
    SystemCoreClockUpdate();
    systick_config();
    update_perf_counter();
    bsp_led_init();
    bsp_btn_init();
    bsp_usart_init();
    memset(usart0_rxbuffer, 0, sizeof(usart0_rxbuffer));
    memset(usart2_rxbuffer, 0, sizeof(usart2_rxbuffer));
    bsp_oled_init();
    OLED_Init();
    bsp_adc_init();
    bsp_dac_init();
    bsp_gd25qxx_init();
    bsp_gd30ad3344_init();
    GD30AD3344_ConfigExternalReference();
    GD30AD3344_Init();
}

/************************************************************
 * Comment  :       进入深度睡眠模式
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_enter_deepsleep(void)
{
    rcu_periph_clock_enable(RCU_PMU);

    __disable_irq();

    bsp_usart_disable_for_deepsleep();
    bsp_oled_disable_for_deepsleep();
    bsp_spi_disable_for_deepsleep();

    adc_disable(ADC0);
    adc_dma_mode_disable(ADC0);
    dma_channel_disable(DMA1, DMA_CH0);
    dac_disable(DAC0, DAC_OUT0);
    dac_dma_disable(DAC0, DAC_OUT0);
    dma_channel_disable(DMA0, DMA_CH5);
    timer_disable(TIMER5);

    bsp_gpio_enter_deepsleep_state();
    bsp_wkup_key_exti_init();

    pmu_flag_clear(PMU_FLAG_RESET_WAKEUP);
    pmu_flag_clear(PMU_FLAG_RESET_STANDBY);

    before_cycle_counter_reconfiguration();
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;

    __enable_irq();

    pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, PMU_LOWDRIVER_ENABLE, WFI_CMD);

    bsp_deepsleep_reinit_after_wakeup();
}

/************************************************************
 * Comment  :       使用 RTC 唤醒定时器的深度睡眠
 * Parameter:       seconds - 唤醒延时秒数
 * Return   :       none
************************************************************/
void bsp_enter_deepsleep_rtc_wakeup(uint16_t seconds)
{
    if (seconds == 0U) seconds = 1U;

    rcu_periph_clock_enable(RCU_PMU);
    __disable_irq();

    bsp_usart_disable_for_deepsleep();
    bsp_oled_disable_for_deepsleep();
    bsp_spi_disable_for_deepsleep();

    adc_disable(ADC0);
    adc_dma_mode_disable(ADC0);
    dma_channel_disable(DMA1, DMA_CH0);
    dac_disable(DAC0, DAC_OUT0);
    dac_dma_disable(DAC0, DAC_OUT0);
    dma_channel_disable(DMA0, DMA_CH5);
    timer_disable(TIMER5);

    bsp_gpio_enter_deepsleep_state();

    exti_interrupt_flag_clear(EXTI_22);
    rtc_flag_clear(RTC_FLAG_WT);
    rtc_wakeup_disable();
    rtc_wakeup_clock_set(WAKEUP_CKSPRE);
    rtc_wakeup_timer_set((uint16_t)(seconds - 1U));
    rtc_interrupt_enable(RTC_INT_WAKEUP);
    exti_init(EXTI_22, EXTI_INTERRUPT, EXTI_TRIG_RISING);
    nvic_irq_enable(RTC_WKUP_IRQn, 1U, 0U);
    rtc_wakeup_enable();

    pmu_flag_clear(PMU_FLAG_RESET_WAKEUP);
    pmu_flag_clear(PMU_FLAG_RESET_STANDBY);

    before_cycle_counter_reconfiguration();
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;

    __enable_irq();

    pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, PMU_LOWDRIVER_ENABLE, WFI_CMD);

    nvic_irq_disable(RTC_WKUP_IRQn);
    exti_interrupt_flag_clear(EXTI_22);
    rtc_wakeup_disable();
    rtc_interrupt_disable(RTC_INT_WAKEUP);
    rtc_flag_clear(RTC_FLAG_WT);

    bsp_deepsleep_reinit_after_wakeup();
}

/************************************************************
 * Comment  :       初始化 USART0（调试串口，115200 波特率，循环 DMA）
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_usart0_init(void)
{
    dma_single_data_parameter_struct dma_init_struct;

    rcu_periph_clock_enable(RCU_DMA1);
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_USART0);

    dma_deinit(DMA1, DMA_CH5);
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.memory0_addr = (uint32_t)usart0_rxbuffer;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.number = sizeof(usart0_rxbuffer);
    dma_init_struct.periph_addr = USART0_RDATA_ADDRESS;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(DMA1, DMA_CH5, &dma_init_struct);

    dma_circulation_enable(DMA1, DMA_CH5);
    dma_channel_subperipheral_select(DMA1, DMA_CH5, DMA_SUBPERI4);
    dma_channel_enable(DMA1, DMA_CH5);

    gpio_af_set(USART0_TX_PORT, AF_USART0, USART0_TX_PIN | USART0_RX_PIN);
    gpio_mode_set(USART0_TX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART0_TX_PIN | USART0_RX_PIN);
    gpio_output_options_set(USART0_TX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART0_TX_PIN | USART0_RX_PIN);

    usart_deinit(USART0);
    usart_baudrate_set(USART0, 115200U);
    usart_receive_config(USART0, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
    usart_dma_receive_config(USART0, USART_RECEIVE_DMA_ENABLE);
    usart_enable(USART0);

    nvic_irq_enable(USART0_IRQn, 0, 0);
    usart_interrupt_enable(USART0, USART_INT_IDLE);
}

/************************************************************
 * Comment  :       初始化所有 USART（USART0 + USART1 OTA）
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_usart_init(void)
{
    bsp_usart0_init();
    bsp_usart1_ota_init();
}

/************************************************************
 * Comment  :       初始化 USART1（OTA + RS485，19200 波特率，循环 DMA）
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_usart1_ota_init(void)
{
    dma_single_data_parameter_struct dma_init_struct;

    rcu_periph_clock_enable(OTA_UART_DMA_RCU);
    rcu_periph_clock_enable(OTA_UART_PORT_RCU);
    rcu_periph_clock_enable(OTA_UART_RCU);

    dma_deinit(OTA_UART_DMA, OTA_UART_DMA_CH);
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.memory0_addr = (uint32_t)rxbuffer;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.number = OTA_UART_RXBUF_SIZE;
    dma_init_struct.periph_addr = OTA_UART_RDATA_ADDRESS;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(OTA_UART_DMA, OTA_UART_DMA_CH, &dma_init_struct);

    dma_circulation_enable(OTA_UART_DMA, OTA_UART_DMA_CH);
    dma_channel_subperipheral_select(OTA_UART_DMA, OTA_UART_DMA_CH, OTA_UART_DMA_SUBPERI);
    dma_channel_enable(OTA_UART_DMA, OTA_UART_DMA_CH);

    gpio_af_set(OTA_UART_PORT, OTA_UART_AF, OTA_UART_TX_PIN | OTA_UART_RX_PIN);
    gpio_mode_set(OTA_UART_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, OTA_UART_TX_PIN | OTA_UART_RX_PIN);
    gpio_output_options_set(OTA_UART_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, OTA_UART_TX_PIN | OTA_UART_RX_PIN);

    usart_deinit(OTA_UART_PERIPH);
    usart_baudrate_set(OTA_UART_PERIPH, OTA_UART_BAUDRATE);
    usart_receive_config(OTA_UART_PERIPH, USART_RECEIVE_ENABLE);
    usart_transmit_config(OTA_UART_PERIPH, USART_TRANSMIT_ENABLE);
    usart_dma_receive_config(OTA_UART_PERIPH, USART_RECEIVE_DMA_ENABLE);
    usart_enable(OTA_UART_PERIPH);

    rcu_periph_clock_enable(RS485_CS_PORT_RCU);
    gpio_mode_set(RS485_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, RS485_CS_PIN);
    gpio_output_options_set(RS485_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, RS485_CS_PIN);
    RS485_CS_SET(0);
}

void bsp_usart1_init(void)
{
    bsp_usart1_ota_init();
}

/************************************************************
 * Comment  :       动态修改 USART1 波特率
 * Parameter:       baudrate - 新的波特率值
 * Return   :       none
************************************************************/
void bsp_usart1_ota_init_with_baud(uint32_t baudrate)
{
    usart_disable(OTA_UART_PERIPH);
    usart_baudrate_set(OTA_UART_PERIPH, baudrate);
    usart_enable(OTA_UART_PERIPH);
}

/************************************************************
 * Comment  :       初始化 USART2
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_usart2_init(void)
{
    dma_single_data_parameter_struct dma_init_struct;

    rcu_periph_clock_enable(RCU_DMA0);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_USART2);

    dma_deinit(USART2_RX_DMA_PERIPH, USART2_RX_DMA_CHANNEL);
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.memory0_addr = (uint32_t)usart2_rxbuffer;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.number = sizeof(usart2_rxbuffer);
    dma_init_struct.periph_addr = USART2_RDATA_ADDRESS;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(USART2_RX_DMA_PERIPH, USART2_RX_DMA_CHANNEL, &dma_init_struct);

    dma_circulation_disable(USART2_RX_DMA_PERIPH, USART2_RX_DMA_CHANNEL);
    dma_channel_subperipheral_select(USART2_RX_DMA_PERIPH, USART2_RX_DMA_CHANNEL, USART2_RX_DMA_SUBPERI);
    dma_channel_enable(USART2_RX_DMA_PERIPH, USART2_RX_DMA_CHANNEL);

    gpio_af_set(USART2_TX_PORT, AF_USART2, USART2_TX_PIN | USART2_RX_PIN);
    gpio_mode_set(USART2_TX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART2_TX_PIN | USART2_RX_PIN);
    gpio_output_options_set(USART2_TX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART2_TX_PIN | USART2_RX_PIN);

    usart_deinit(USART2);
    usart_baudrate_set(USART2, 115200U);
    usart_receive_config(USART2, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART2, USART_TRANSMIT_ENABLE);
    usart_dma_receive_config(USART2, USART_RECEIVE_DMA_ENABLE);
    usart_enable(USART2);

    nvic_irq_enable(USART2_IRQn, 1U, 0U);
    usart_interrupt_enable(USART2, USART_INT_IDLE);
}

/************************************************************
 * Comment  :       重新配置 OTA UART DMA（传输完成后重载）
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_ota_uart_dma_rearm(void)
{
    dma_channel_disable(OTA_UART_DMA, OTA_UART_DMA_CH);
    dma_flag_clear(OTA_UART_DMA, OTA_UART_DMA_CH, DMA_FLAG_FTF);
    dma_transfer_number_config(OTA_UART_DMA, OTA_UART_DMA_CH, OTA_UART_RXBUF_SIZE);
    dma_channel_enable(OTA_UART_DMA, OTA_UART_DMA_CH);
}

/************************************************************
 * Comment  :       获取 OTA UART DMA 已接收字节数
 * Parameter:       none
 * Return   :       已接收字节数
************************************************************/
uint32_t bsp_ota_uart_dma_received_len(void)
{
    uint32_t remaining = dma_transfer_number_get(OTA_UART_DMA, OTA_UART_DMA_CH);
    if (remaining == 0U)
    {
        return OTA_UART_RXBUF_SIZE;
    }
    return (OTA_UART_RXBUF_SIZE - remaining);
}

/************************************************************
 * Comment  :       初始化 USART5
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_usart5_init(void)
{
    dma_single_data_parameter_struct dma_init_struct;

    rcu_periph_clock_enable(RCU_DMA1);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_USART5);

    dma_deinit(USART5_RX_DMA_PERIPH, USART5_RX_DMA_CHANNEL);
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.memory0_addr = (uint32_t)usart5_rxbuffer;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.number = sizeof(usart5_rxbuffer);
    dma_init_struct.periph_addr = USART5_RDATA_ADDRESS;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(USART5_RX_DMA_PERIPH, USART5_RX_DMA_CHANNEL, &dma_init_struct);

    dma_circulation_disable(USART5_RX_DMA_PERIPH, USART5_RX_DMA_CHANNEL);
    dma_channel_subperipheral_select(USART5_RX_DMA_PERIPH, USART5_RX_DMA_CHANNEL, USART5_RX_DMA_SUBPERI);
    dma_channel_enable(USART5_RX_DMA_PERIPH, USART5_RX_DMA_CHANNEL);

    gpio_af_set(USART5_TX_PORT, AF_USART5, USART5_TX_PIN | USART5_RX_PIN);
    gpio_mode_set(USART5_TX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART5_TX_PIN | USART5_RX_PIN);
    gpio_output_options_set(USART5_TX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART5_TX_PIN | USART5_RX_PIN);

    usart_deinit(USART5);
    usart_baudrate_set(USART5, 115200U);
    usart_receive_config(USART5, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART5, USART_TRANSMIT_ENABLE);
    usart_dma_receive_config(USART5, USART_RECEIVE_DMA_ENABLE);
    usart_enable(USART5);
}

void bsp_usart_all_init(void)
{
    bsp_usart0_init();
    bsp_usart1_init();
    bsp_usart2_init();
    bsp_usart5_init();
}

/************************************************************
 * Comment  :       初始化 OLED（I2C0，400kHz，DMA 发送）
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_oled_init(void)
{
    dma_single_data_parameter_struct dma_init_struct;
    rcu_periph_clock_enable(OLED_CLK_PORT);
    rcu_periph_clock_enable(RCU_I2C0);
    rcu_periph_clock_enable(RCU_DMA0);

    i2c_deinit(I2C0);

    gpio_af_set(OLED_PORT, AF_OLED_I2C0, OLED_DAT_PIN);
    gpio_af_set(OLED_PORT, AF_OLED_I2C0, OLED_CLK_PIN);

    gpio_mode_set(OLED_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, OLED_DAT_PIN);
    gpio_output_options_set(OLED_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, OLED_DAT_PIN);
    gpio_mode_set(OLED_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, OLED_CLK_PIN);
    gpio_output_options_set(OLED_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, OLED_CLK_PIN);

    i2c_clock_config(I2C0, 400000, I2C_DTCY_2);
    i2c_mode_addr_config(I2C0, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, I2C0_OWN_ADDRESS7);
    i2c_enable(I2C0);
    i2c_ack_config(I2C0, I2C_ACK_ENABLE);

    dma_deinit(DMA0, DMA_CH6);

    dma_single_data_para_struct_init(&dma_init_struct);
    dma_init_struct.direction = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.memory0_addr = (uint32_t)oled_data_buf;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.number = 2;
    dma_init_struct.periph_addr = I2C0_DATA_ADDRESS;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(DMA0, DMA_CH6, &dma_init_struct);

    dma_circulation_disable(DMA0, DMA_CH6);
    dma_channel_subperipheral_select(DMA0, DMA_CH6, DMA_SUBPERI1);
}

/************************************************************
 * Comment  :       初始化 SPI Flash（SPI0，全双工主模式）
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_gd25qxx_init(void)
{
    rcu_periph_clock_enable(SPI_CLK_PORT);
    rcu_periph_clock_enable(SPI_CS_CLK_PORT);
    rcu_periph_clock_enable(RCU_SPI0);
    rcu_periph_clock_enable(RCU_DMA0);

    gpio_af_set(SPI_PORT, AF_SPI_FLASH, SPI_SCK | SPI_MISO | SPI_MOSI);
    gpio_mode_set(SPI_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SPI_SCK | SPI_MISO | SPI_MOSI);
    gpio_output_options_set(SPI_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SPI_SCK | SPI_MISO | SPI_MOSI);

    gpio_mode_set(SPI_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI_NSS);
    gpio_output_options_set(SPI_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SPI_NSS);

    spi_parameter_struct spi_init_struct;

    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_HIGH_PH_2EDGE;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = SPI_PSC_8;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(SPI0, &spi_init_struct);

    spi_flash_init();
}

/************************************************************
 * Comment  :       初始化 GD30AD3344 ADC（SPI3，全双工主模式）
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_gd30ad3344_init(void)
{
    rcu_periph_clock_enable(GD30_SPI_PORT_RCU);
    rcu_periph_clock_enable(GD30_CS_PORT_RCU);
    rcu_periph_clock_enable(GD30_SPI_RCU);
    rcu_periph_clock_enable(GD30_DMA_RCU);

    gpio_af_set(GD30_SPI_PORT, AF_SPI3, GD30_SPI_SCK | GD30_SPI_MISO | GD30_SPI_MOSI);
    gpio_mode_set(GD30_SPI_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, GD30_SPI_SCK | GD30_SPI_MISO | GD30_SPI_MOSI);
    gpio_output_options_set(GD30_SPI_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GD30_SPI_SCK | GD30_SPI_MISO | GD30_SPI_MOSI);

    gpio_mode_set(GD30_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GD30_CS_PIN);
    gpio_output_options_set(GD30_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GD30_CS_PIN);
    GD30_CS_HIGH();

    spi_parameter_struct spi_init_struct;
    spi_i2s_deinit(GD30_SPI);
    spi_struct_para_init(&spi_init_struct);

    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = GD30_SPIMODE;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = GD30_SPI_PRESCALE;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(GD30_SPI, &spi_init_struct);
    spi_enable(GD30_SPI);
}

/************************************************************
 * Comment  :       初始化以太网 RMII 接口 GPIO
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_eth_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);

    rcu_periph_clock_enable(RCU_SYSCFG);
    rcu_periph_clock_enable(RCU_ENET);
    rcu_periph_clock_enable(RCU_ENETTX);
    rcu_periph_clock_enable(RCU_ENETRX);

    syscfg_enet_phy_interface_config(SYSCFG_ENET_PHY_RMII);

    gpio_af_set(ETH_REF_CLK_PORT, AF_ETH, ETH_REF_CLK_PIN | ETH_MDIO_PIN | ETH_CRS_DV_PIN);
    gpio_mode_set(ETH_REF_CLK_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, ETH_REF_CLK_PIN | ETH_MDIO_PIN | ETH_CRS_DV_PIN);
    gpio_output_options_set(ETH_REF_CLK_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, ETH_REF_CLK_PIN | ETH_MDIO_PIN | ETH_CRS_DV_PIN);

    gpio_af_set(ETH_MDC_PORT, AF_ETH, ETH_MDC_PIN | ETH_RXD0_PIN | ETH_RXD1_PIN);
    gpio_mode_set(ETH_MDC_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, ETH_MDC_PIN | ETH_RXD0_PIN | ETH_RXD1_PIN);
    gpio_output_options_set(ETH_MDC_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, ETH_MDC_PIN | ETH_RXD0_PIN | ETH_RXD1_PIN);

    gpio_af_set(ETH_TX_EN_PORT, AF_ETH, ETH_TX_EN_PIN | ETH_TXD0_PIN | ETH_TXD1_PIN);
    gpio_mode_set(ETH_TX_EN_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, ETH_TX_EN_PIN | ETH_TXD0_PIN | ETH_TXD1_PIN);
    gpio_output_options_set(ETH_TX_EN_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, ETH_TX_EN_PIN | ETH_TXD0_PIN | ETH_TXD1_PIN);

    gpio_mode_set(PHY_RST_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, PHY_RST_PIN);
    gpio_output_options_set(PHY_RST_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, PHY_RST_PIN);
    gpio_bit_set(PHY_RST_PORT, PHY_RST_PIN);
}

/************************************************************
 * Comment  :       初始化 ADC0（连续扫描模式，CH10/CH11，DMA 循环）
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_adc_init(void)
{
    rcu_periph_clock_enable(ADC1_CLK_PORT);
    rcu_periph_clock_enable(RCU_ADC0);
    rcu_periph_clock_enable(ADC_DMA_RCU);

    adc_clock_config(ADC_ADCCK_PCLK2_DIV8);

    gpio_mode_set(ADC1_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, ADC1_PIN | GPIO_PIN_1);

    dma_single_data_parameter_struct dma_single_data_parameter;

    dma_deinit(ADC_DMA, ADC_DMA_CHANNEL);

    dma_single_data_parameter.periph_addr = (uint32_t)(&ADC_RDATA(ADC0));
    dma_single_data_parameter.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_single_data_parameter.memory0_addr = (uint32_t)(adc_value);
    dma_single_data_parameter.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_single_data_parameter.periph_memory_width = DMA_PERIPH_WIDTH_16BIT;
    dma_single_data_parameter.direction = DMA_PERIPH_TO_MEMORY;
    dma_single_data_parameter.number = 2;
    dma_single_data_parameter.priority = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(ADC_DMA, ADC_DMA_CHANNEL, &dma_single_data_parameter);
    dma_channel_subperipheral_select(ADC_DMA, ADC_DMA_CHANNEL, ADC_DMA_SUBPERI);

    dma_circulation_enable(ADC_DMA, ADC_DMA_CHANNEL);
    dma_channel_enable(ADC_DMA, ADC_DMA_CHANNEL);

    adc_sync_mode_config(ADC_SYNC_MODE_INDEPENDENT);
    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, ENABLE);
    adc_special_function_config(ADC0, ADC_SCAN_MODE, ENABLE);
    adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);

    adc_channel_length_config(ADC0, ADC_ROUTINE_CHANNEL, 2);
    adc_routine_channel_config(ADC0, 0, ADC_CHANNEL_10, ADC_SAMPLETIME_15);
    adc_routine_channel_config(ADC0, 1, ADC_CHANNEL_11, ADC_SAMPLETIME_15);
    adc_external_trigger_source_config(ADC0, ADC_ROUTINE_CHANNEL, ADC_EXTTRIG_ROUTINE_T0_CH0);
    adc_external_trigger_config(ADC0, ADC_ROUTINE_CHANNEL, EXTERNAL_TRIGGER_DISABLE);

    adc_dma_request_after_last_enable(ADC0);
    adc_dma_mode_enable(ADC0);

    adc_enable(ADC0);
    delay_1ms(1);
    adc_calibration_enable(ADC0);

    adc_software_trigger_enable(ADC0, ADC_ROUTINE_CHANNEL);
}

/************************************************************
 * Comment  :       配置 Timer5（DAC 触发源，240 预分频，99 周期）
 * Parameter:       none
 * Return   :       none
************************************************************/
void timer5_config(void)
{
    timer_parameter_struct timer_initpara;

    timer_deinit(TIMER5);

    timer_struct_para_init(&timer_initpara);
    timer_initpara.prescaler         = 239;
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = 99;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;

    timer_init(TIMER5, &timer_initpara);

    timer_master_output_trigger_source_select(TIMER5, TIMER_TRI_OUT_SRC_UPDATE);

    timer_enable(TIMER5);
}

/************************************************************
 * Comment  :       初始化 DAC0（Timer5 TRGO 触发，PA4 输出）
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_dac_init(void)
{
    rcu_periph_clock_enable(DAC1_CLK_PORT);
    rcu_periph_clock_enable(RCU_DAC);
    rcu_periph_clock_enable(RCU_TIMER5);

    gpio_mode_set(DAC1_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, DAC1_PIN);

    dac_deinit(DAC0);
    dac_trigger_source_config(DAC0, DAC_OUT0, DAC_TRIGGER_T5_TRGO);
    dac_trigger_enable(DAC0, DAC_OUT0);
    dac_wave_mode_config(DAC0, DAC_OUT0, DAC_WAVE_DISABLE);

    dac_enable(DAC0, DAC_OUT0);

    timer5_config();
}

/************************************************************
 * Comment  :       RTC 设置（初始化时间和日期）
 * Parameter:       none
 * Return   :       0  - 成功
 *                  -1 - 失败
************************************************************/
int bsp_rtc_setup(void)
{
    uint32_t hh = 0x12, mm = 0x30, ss = 0x00;
    rtc_initpara.factor_asyn = prescaler_a;
    rtc_initpara.factor_syn  = prescaler_s;
    rtc_initpara.year        = 0x26;
    rtc_initpara.day_of_week = 0x03;
    rtc_initpara.month       = RTC_MAY;
    rtc_initpara.date        = 0x27;
    rtc_initpara.display_format = RTC_24HOUR;
    rtc_initpara.am_pm       = RTC_AM;
    rtc_initpara.hour        = hh;
    rtc_initpara.minute      = mm;
    rtc_initpara.second      = ss;

    if (rtc_init(&rtc_initpara) == ERROR)
    {
        return -1;
    }
    RTC_BKP0 = BKP_VALUE;
    return 0;
}

/************************************************************
 * Comment  :       RTC 预配置（时钟源选择和预分频）
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_rtc_pre_cfg(void)
{
    #if defined (RTC_CLOCK_SOURCE_IRC32K)
          rcu_osci_on(RCU_IRC32K);
          rcu_osci_stab_wait(RCU_IRC32K);
          rcu_rtc_clock_config(RCU_RTCSRC_IRC32K);

          prescaler_s = 0x13F;
          prescaler_a = 0x63;
    #elif defined (RTC_CLOCK_SOURCE_LXTAL)
          rcu_osci_on(RCU_LXTAL);
          rcu_osci_stab_wait(RCU_LXTAL);
          rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);

          prescaler_s = 0xFF;
          prescaler_a = 0x7F;
    #else
    #error RTC clock source should be defined.
    #endif

    rcu_periph_clock_enable(RCU_RTC);
    rtc_register_sync_wait();
}

/************************************************************
 * Comment  :       RTC 初始化
 * Parameter:       none
 * Return   :       0 - 成功
************************************************************/
int bsp_rtc_init(void)
{
    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();

    bsp_rtc_pre_cfg();
    RTCSRC_FLAG = GET_BITS(RCU_BDCTL, 8, 9);

    bsp_rtc_setup();

    rcu_all_reset_flag_clear();
    return 0;
}

/************************************************************
 * Comment  :       EXTI0 中断处理（WKUP 按键中断）
 * Parameter:       none
 * Return   :       none
************************************************************/
void EXTI0_IRQHandler(void)
{
    if (RESET != exti_interrupt_flag_get(EXTI_0))
    {
        exti_interrupt_flag_clear(EXTI_0);
    }
}

/************************************************************
 * Comment  :       RTC 唤醒定时器中断处理
 * Parameter:       none
 * Return   :       none
************************************************************/
void RTC_WKUP_IRQHandler(void)
{
    if (RESET != exti_interrupt_flag_get(EXTI_22))
    {
        exti_interrupt_flag_clear(EXTI_22);
    }
    if (SET == rtc_flag_get(RTC_FLAG_WT))
    {
        rtc_flag_clear(RTC_FLAG_WT);
    }
}

/************************************************************
 * Comment  :       初始化 KEY1 EXTI（PC13 下降沿，用于 Sleep 唤醒）
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_key1_exti_init(void)
{
    rcu_periph_clock_enable(RCU_SYSCFG);
    rcu_periph_clock_enable(KEYC_CLK_PORT);

    gpio_mode_set(KEYC_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, KEY1_PIN);

    syscfg_exti_line_config(EXTI_SOURCE_GPIOC, EXTI_SOURCE_PIN13);
    exti_init(EXTI_13, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_interrupt_flag_clear(EXTI_13);
    nvic_irq_enable(EXTI10_15_IRQn, 2U, 0U);
}

/************************************************************
 * Comment  :       进入 Sleep 模式（CPU 停，外设保持，KEY1 唤醒）
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_enter_sleepmode(void)
{
    rcu_periph_clock_enable(RCU_PMU);
    bsp_key1_exti_init();
    pmu_to_sleepmode(WFI_CMD);
    exti_interrupt_disable(EXTI_13);
    nvic_irq_disable(EXTI10_15_IRQn);
}

/************************************************************
 * Comment  :       进入 Standby 模式（PA0 WKUP 唤醒，等同复位）
 * Parameter:       none
 * Return   :       none
************************************************************/
void bsp_enter_standby_wkup(void)
{
    rcu_periph_clock_enable(RCU_PMU);

    pmu_flag_clear(PMU_FLAG_RESET_WAKEUP);
    pmu_flag_clear(PMU_FLAG_RESET_STANDBY);
    pmu_wakeup_pin_enable();

    pmu_to_standbymode();
}

/************************************************************
 * Comment  :       assert 失败处理函数
 * Parameter:       expr - 失败表达式
 *                  file - 源文件名
 *                  line - 行号
 * Return   :       none
************************************************************/
void __aeabi_assert(const char *expr, const char *file, int line)
{
    my_printf(rs485_uart,
              "ASSERT: %s, file: %s, line: %d\r\n",
              (NULL != expr) ? expr : "?",
              (NULL != file) ? file : "?",
              line);
    while (1)
    {
    }
}
