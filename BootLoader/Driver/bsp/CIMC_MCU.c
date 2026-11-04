
#include "CIMC_MCU.h"

/* USART1 RX DMA 缓冲区 */
uint8_t rxbuffer[OTA_UART_RXBUF_SIZE];

void bsp_wkup_key_exti_init(void)
{
    rcu_periph_clock_enable(WK_UP_CLK_PORT);
    rcu_periph_clock_enable(RCU_SYSCFG);

    gpio_mode_set(WK_UP_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, WK_UP_PIN);

    syscfg_exti_line_config(EXTI_SOURCE_GPIOA, EXTI_SOURCE_PIN0);
    exti_init(EXTI_0, EXTI_INTERRUPT, EXTI_TRIG_BOTH);
    exti_interrupt_flag_clear(EXTI_0);
    nvic_irq_enable(EXTI0_IRQn, 1U, 0U);
}

static void bsp_usart_disable_for_deepsleep(void)
{
    usart_interrupt_disable(USART1, USART_INT_IDLE);
    nvic_irq_disable(USART1_IRQn);
    usart_dma_receive_config(USART1, USART_RECEIVE_DMA_DISABLE);
    dma_channel_disable(OTA_UART_DMA, OTA_UART_DMA_CH);
    usart_disable(USART1);
}

static void bsp_gpio_enter_deepsleep_state(void)
{
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOE);
    rcu_periph_clock_enable(RCU_GPIOA);

    gpio_mode_set(USART1_TX_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, USART1_TX_PIN);
    gpio_mode_set(USART1_RX_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, USART1_RX_PIN);
    gpio_mode_set(RS485_CS_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, RS485_CS_PIN);
    gpio_mode_set(WK_UP_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, WK_UP_PIN);
}

static void bsp_deepsleep_reinit_after_wakeup(void)
{
    SystemInit();
    SystemCoreClockUpdate();
    systick_config();
    bsp_usart_init();
}

void bsp_enter_deepsleep(void)
{
    rcu_periph_clock_enable(RCU_PMU);

    __disable_irq();

    bsp_usart_disable_for_deepsleep();
    bsp_gpio_enter_deepsleep_state();
    bsp_wkup_key_exti_init();

    pmu_flag_clear(PMU_FLAG_RESET_WAKEUP);
    pmu_flag_clear(PMU_FLAG_RESET_STANDBY);

    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;

    __enable_irq();

    pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, PMU_LOWDRIVER_ENABLE, WFI_CMD);

    bsp_deepsleep_reinit_after_wakeup();
}

/*!
    \brief      configure USART1 (PD5 TX / PD6 RX) with RS485 direction control (PE8)
    \param[in]  none
    \param[out] none
    \retval     none
*/
void bsp_usart1_init(void)
{
    dma_single_data_parameter_struct dma_init_struct;

    rcu_periph_clock_enable(RCU_DMA0);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_USART1);

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

    /* USART1 TX (PD5) and RX (PD6) */
    gpio_af_set(USART1_TX_PORT, AF_USART1, USART1_TX_PIN);
    gpio_af_set(USART1_RX_PORT, AF_USART1, USART1_RX_PIN);

    gpio_mode_set(USART1_TX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART1_TX_PIN);
    gpio_output_options_set(USART1_TX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART1_TX_PIN);
    gpio_mode_set(USART1_RX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, USART1_RX_PIN);
    gpio_output_options_set(USART1_RX_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, USART1_RX_PIN);

    usart_deinit(USART1);
    usart_baudrate_set(USART1, 19200U);
    usart_receive_config(USART1, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);
    usart_dma_receive_config(USART1, USART_RECEIVE_DMA_ENABLE);
    usart_enable(USART1);

    /* RS485 direction control PE8, default LOW = RX mode */
    rcu_periph_clock_enable(RS485_CS_PORT_RCU);
    gpio_mode_set(RS485_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLDOWN, RS485_CS_PIN);
    gpio_output_options_set(RS485_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, RS485_CS_PIN);
    RS485_CS_SET(0);
}

void bsp_usart_init(void)
{
    bsp_usart1_init();
}

/* OLED I2C0 init — minimal for BL display */
void bsp_oled_init(void)
{
    rcu_periph_clock_enable(OLED_CLK_PORT);
    rcu_periph_clock_enable(RCU_I2C0);

    gpio_af_set(OLED_PORT, AF_OLED_I2C0, OLED_DAT_PIN | OLED_CLK_PIN);
    gpio_mode_set(OLED_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, OLED_DAT_PIN | OLED_CLK_PIN);
    gpio_output_options_set(OLED_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, OLED_DAT_PIN | OLED_CLK_PIN);

    i2c_clock_config(I2C0, 400000, I2C_DTCY_2);
    i2c_mode_addr_config(I2C0, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, I2C0_OWN_ADDRESS7);
    i2c_enable(I2C0);
    i2c_ack_config(I2C0, I2C_ACK_ENABLE);
}

/* Simple OLED display function used by BL core */
void oled_show_bootloader(void)
{
    OLED_Init();
    OLED_ShowStr(0, 0, "Bootloader     ", 8);
}
