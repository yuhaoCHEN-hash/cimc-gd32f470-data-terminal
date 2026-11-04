/*!
    \file    gd30ad3344.c
    \brief   gd30ad3344 driver

    \version 2024-10-08, V1.0.0, firmware for GD30AD3344
*/

// 头文件
#include "gd30ad3344.h"

// 变量声明
extern uint8_t spi3_send_array[ARRAYSIZE];
extern uint8_t spi3_receive_array[ARRAYSIZE];

// 函数声明
static void spi_gd30ad3344_config_spi(uint32_t frame_size);
static uint16_t spi_gd30ad3344_send_word_pair_dma(uint16_t first_word, uint16_t second_word);
static uint16_t spi_gd30ad3344_send_word_triple_dma(uint16_t first_word, uint16_t second_word, uint16_t third_word,
                                                    uint16_t *first_rx, uint16_t *second_rx);

// 函数定义

/************************************************************
 * Comment  :       通过 DMA 发送并接收一个字节
 * Parameter:       byte - 要发送的字节
 * Return   :       从 SPI 总线接收到的字节
************************************************************/
uint8_t spi_gd30ad3344_send_byte_dma(uint8_t byte)
{
    spi3_send_array[0] = byte;

    dma_single_data_parameter_struct dma_init_struct;

    dma_deinit(GD30_DMA, GD30_DMA_CHANNEL_TX);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(GD30_SPI);
    dma_init_struct.memory0_addr        = (uint32_t)spi3_send_array;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_init_struct.number              = 1;
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(GD30_DMA, GD30_DMA_CHANNEL_TX, &dma_init_struct);
    dma_channel_subperipheral_select(GD30_DMA, GD30_DMA_CHANNEL_TX, GD30_DMA_SUBPERI);

    dma_deinit(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(GD30_SPI);
    dma_init_struct.memory0_addr        = (uint32_t)spi3_receive_array;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(GD30_DMA, GD30_DMA_CHANNEL_RX, &dma_init_struct);
    dma_channel_subperipheral_select(GD30_DMA, GD30_DMA_CHANNEL_RX, GD30_DMA_SUBPERI);

    dma_channel_enable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_enable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    spi_dma_enable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_enable(GD30_SPI, SPI_DMA_TRANSMIT);

    while (RESET == dma_flag_get(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF));
    while (RESET == dma_flag_get(GD30_DMA, GD30_DMA_CHANNEL_TX, DMA_FLAG_FTF));
    while (SET == spi_i2s_flag_get(GD30_SPI, SPI_FLAG_TRANS));

    spi_dma_disable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_disable(GD30_SPI, SPI_DMA_TRANSMIT);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF);
    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_TX, DMA_FLAG_FTF);

    return spi3_receive_array[0];
}

/************************************************************
 * Comment  :       通过 DMA 发送并接收一个半字（16 位数据）
 * Parameter:       half_word - 要发送的半字
 * Return   :       从 SPI 总线接收到的半字
************************************************************/
uint16_t spi_gd30ad3344_send_halfword_dma(uint16_t half_word)
{
    GD30_CS_LOW();
    uint16_t rx_data;

    spi3_send_array[0] = (uint8_t)(half_word >> 8);
    spi3_send_array[1] = (uint8_t)half_word;

    dma_single_data_parameter_struct dma_init_struct;

    dma_deinit(GD30_DMA, GD30_DMA_CHANNEL_TX);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(GD30_SPI);
    dma_init_struct.memory0_addr        = (uint32_t)spi3_send_array;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_init_struct.number              = 2;
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(GD30_DMA, GD30_DMA_CHANNEL_TX, &dma_init_struct);
    dma_channel_subperipheral_select(GD30_DMA, GD30_DMA_CHANNEL_TX, GD30_DMA_SUBPERI);

    dma_deinit(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(GD30_SPI);
    dma_init_struct.memory0_addr        = (uint32_t)spi3_receive_array;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(GD30_DMA, GD30_DMA_CHANNEL_RX, &dma_init_struct);
    dma_channel_subperipheral_select(GD30_DMA, GD30_DMA_CHANNEL_RX, GD30_DMA_SUBPERI);

    dma_channel_enable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_enable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    spi_dma_enable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_enable(GD30_SPI, SPI_DMA_TRANSMIT);

    while (RESET == dma_flag_get(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF));
    while (RESET == dma_flag_get(GD30_DMA, GD30_DMA_CHANNEL_TX, DMA_FLAG_FTF));
    while (SET == spi_i2s_flag_get(GD30_SPI, SPI_FLAG_TRANS));

    spi_dma_disable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_disable(GD30_SPI, SPI_DMA_TRANSMIT);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF);
    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_TX, DMA_FLAG_FTF);

    rx_data = (uint16_t)(spi3_receive_array[0] << 8);
    rx_data |= spi3_receive_array[1];
    GD30_CS_HIGH();
    return rx_data;
}

/************************************************************
 * Comment  :       通过 DMA 发送并接收多个字节
 * Parameter:       tx_buffer - 发送缓冲区
 *                  rx_buffer - 接收缓冲区
 *                  size      - 传输大小
 * Return   :       none
************************************************************/
void spi_gd30ad3344_transmit_receive_dma(uint8_t *tx_buffer, uint8_t *rx_buffer, uint16_t size)
{
    if (size > ARRAYSIZE)
    {
        size = ARRAYSIZE;
    }

    for (uint16_t i = 0; i < size; i++)
    {
        spi3_send_array[i] = tx_buffer[i];
    }

    dma_single_data_parameter_struct dma_init_struct;

    dma_deinit(GD30_DMA, GD30_DMA_CHANNEL_TX);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(GD30_SPI);
    dma_init_struct.memory0_addr        = (uint32_t)spi3_send_array;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_init_struct.number              = size;
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(GD30_DMA, GD30_DMA_CHANNEL_TX, &dma_init_struct);
    dma_channel_subperipheral_select(GD30_DMA, GD30_DMA_CHANNEL_TX, GD30_DMA_SUBPERI);

    dma_deinit(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(GD30_SPI);
    dma_init_struct.memory0_addr        = (uint32_t)spi3_receive_array;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(GD30_DMA, GD30_DMA_CHANNEL_RX, &dma_init_struct);
    dma_channel_subperipheral_select(GD30_DMA, GD30_DMA_CHANNEL_RX, GD30_DMA_SUBPERI);

    dma_channel_enable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_enable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    spi_dma_enable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_enable(GD30_SPI, SPI_DMA_TRANSMIT);

    while (RESET == dma_flag_get(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF));
    while (RESET == dma_flag_get(GD30_DMA, GD30_DMA_CHANNEL_TX, DMA_FLAG_FTF));
    while (SET == spi_i2s_flag_get(GD30_SPI, SPI_FLAG_TRANS));

    spi_dma_disable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_disable(GD30_SPI, SPI_DMA_TRANSMIT);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF);
    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_TX, DMA_FLAG_FTF);

    for (uint16_t i = 0; i < size; i++)
    {
        rx_buffer[i] = spi3_receive_array[i];
    }
}

/************************************************************
 * Comment  :       等待 DMA 传输完成
 * Parameter:       none
 * Return   :       none
************************************************************/
void spi_gd30ad3344_wait_for_dma_end(void)
{
    while (RESET == dma_flag_get(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF));
    while (RESET == dma_flag_get(GD30_DMA, GD30_DMA_CHANNEL_TX, DMA_FLAG_FTF));
    while (SET == spi_i2s_flag_get(GD30_SPI, SPI_FLAG_TRANS));

    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF);
    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_TX, DMA_FLAG_FTF);
}

/************************************************************
 * Comment  :       通过 DMA 发送并接收两个字（内部）
 * Parameter:       first_word  - 第一个字
 *                  second_word - 第二个字
 * Return   :       接收到的数据
************************************************************/
static uint16_t spi_gd30ad3344_send_word_pair_dma(uint16_t first_word, uint16_t second_word)
{
    uint16_t rx_data;

    GD30_CS_LOW();

    spi3_send_array[0] = (uint8_t)(first_word >> 8);
    spi3_send_array[1] = (uint8_t)first_word;
    spi3_send_array[2] = (uint8_t)(second_word >> 8);
    spi3_send_array[3] = (uint8_t)second_word;

    dma_single_data_parameter_struct dma_init_struct;

    dma_deinit(GD30_DMA, GD30_DMA_CHANNEL_TX);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(GD30_SPI);
    dma_init_struct.memory0_addr        = (uint32_t)spi3_send_array;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_init_struct.number              = 4;
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(GD30_DMA, GD30_DMA_CHANNEL_TX, &dma_init_struct);
    dma_channel_subperipheral_select(GD30_DMA, GD30_DMA_CHANNEL_TX, GD30_DMA_SUBPERI);

    dma_deinit(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(GD30_SPI);
    dma_init_struct.memory0_addr        = (uint32_t)spi3_receive_array;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(GD30_DMA, GD30_DMA_CHANNEL_RX, &dma_init_struct);
    dma_channel_subperipheral_select(GD30_DMA, GD30_DMA_CHANNEL_RX, GD30_DMA_SUBPERI);

    dma_channel_enable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_enable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    spi_dma_enable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_enable(GD30_SPI, SPI_DMA_TRANSMIT);

    while (RESET == dma_flag_get(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF));
    while (RESET == dma_flag_get(GD30_DMA, GD30_DMA_CHANNEL_TX, DMA_FLAG_FTF));
    while (SET == spi_i2s_flag_get(GD30_SPI, SPI_FLAG_TRANS));

    spi_dma_disable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_disable(GD30_SPI, SPI_DMA_TRANSMIT);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF);
    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_TX, DMA_FLAG_FTF);

    rx_data = (uint16_t)(spi3_receive_array[2] << 8);
    rx_data |= spi3_receive_array[3];

    GD30_CS_HIGH();

    return rx_data;
}

/************************************************************
 * Comment  :       通过 DMA 发送并接收三个字（内部）
 * Parameter:       first_word  - 第一个字
 *                  second_word - 第二个字
 *                  third_word  - 第三个字
 *                  first_rx    - 第一个接收字输出
 *                  second_rx   - 第二个接收字输出
 * Return   :       第三个接收字
************************************************************/
static uint16_t spi_gd30ad3344_send_word_triple_dma(uint16_t first_word, uint16_t second_word, uint16_t third_word,
                                                    uint16_t *first_rx, uint16_t *second_rx)
{
    uint16_t third_rx;

    GD30_CS_LOW();

    spi3_send_array[0] = (uint8_t)(first_word >> 8);
    spi3_send_array[1] = (uint8_t)first_word;
    spi3_send_array[2] = (uint8_t)(second_word >> 8);
    spi3_send_array[3] = (uint8_t)second_word;
    spi3_send_array[4] = (uint8_t)(third_word >> 8);
    spi3_send_array[5] = (uint8_t)third_word;

    dma_single_data_parameter_struct dma_init_struct;

    dma_deinit(GD30_DMA, GD30_DMA_CHANNEL_TX);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(GD30_SPI);
    dma_init_struct.memory0_addr        = (uint32_t)spi3_send_array;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_init_struct.number              = 6;
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(GD30_DMA, GD30_DMA_CHANNEL_TX, &dma_init_struct);
    dma_channel_subperipheral_select(GD30_DMA, GD30_DMA_CHANNEL_TX, GD30_DMA_SUBPERI);

    dma_deinit(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(GD30_SPI);
    dma_init_struct.memory0_addr        = (uint32_t)spi3_receive_array;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(GD30_DMA, GD30_DMA_CHANNEL_RX, &dma_init_struct);
    dma_channel_subperipheral_select(GD30_DMA, GD30_DMA_CHANNEL_RX, GD30_DMA_SUBPERI);

    dma_channel_enable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_enable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    spi_dma_enable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_enable(GD30_SPI, SPI_DMA_TRANSMIT);

    while (RESET == dma_flag_get(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF));
    while (RESET == dma_flag_get(GD30_DMA, GD30_DMA_CHANNEL_TX, DMA_FLAG_FTF));
    while (SET == spi_i2s_flag_get(GD30_SPI, SPI_FLAG_TRANS));

    spi_dma_disable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_disable(GD30_SPI, SPI_DMA_TRANSMIT);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF);
    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_TX, DMA_FLAG_FTF);

    if (first_rx != NULL)
    {
        *first_rx = (uint16_t)(spi3_receive_array[0] << 8);
        *first_rx |= spi3_receive_array[1];
    }
    if (second_rx != NULL)
    {
        *second_rx = (uint16_t)(spi3_receive_array[2] << 8);
        *second_rx |= spi3_receive_array[3];
    }
    third_rx = (uint16_t)(spi3_receive_array[4] << 8);
    third_rx |= spi3_receive_array[5];

    GD30_CS_HIGH();

    return third_rx;
}

/************************************************************
 * Comment  :       配置 SPI 帧大小
 * Parameter:       frame_size - SPI 帧大小
 * Return   :       none
************************************************************/
static void spi_gd30ad3344_config_spi(uint32_t frame_size)
{
    spi_parameter_struct spi_init_struct;

    spi_dma_disable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_disable(GD30_SPI, SPI_DMA_TRANSMIT);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    spi_i2s_deinit(GD30_SPI);
    spi_struct_para_init(&spi_init_struct);

    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = frame_size;
    spi_init_struct.clock_polarity_phase = GD30_SPIMODE;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = GD30_SPI_PRESCALE;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(GD30_SPI, &spi_init_struct);
    spi_enable(GD30_SPI);
    (void)spi_i2s_data_receive(GD30_SPI);
}

// 变量定义
GD30AD3344 GD30AD3344_InitStruct;
uint16_t GD30AD3344_RawCode = 0U;

/************************************************************
 * Comment  :       GD30AD3344 初始化
 * Parameter:       none
 * Return   :       none
************************************************************/
void GD30AD3344_Init(void)
{
    GD30AD3344_InitStruct.SS         = GD30AD3344_OS_DISABLE;
    GD30AD3344_InitStruct.MUX        = GD30AD3344_MUX_AIN0_GND;
    GD30AD3344_InitStruct.PGA        = GD30AD3344_PGA_4V096;
    GD30AD3344_InitStruct.MODE       = GD30AD3344_MODE_CONTINUOUS;
    GD30AD3344_InitStruct.DR         = GD30AD3344_DR_25SPS;
    GD30AD3344_InitStruct.RESERVED_1 = GD30AD3344_RESERVED_0;
    GD30AD3344_InitStruct.PULL_UP_EN = GD30AD3344_PULL_UP_ENABLE;
    GD30AD3344_InitStruct.NOP        = GD30AD3344_NOP_VALID_UPDATE;
    GD30AD3344_InitStruct.RESERVED   = GD30AD3344_RESERVED_1;

    spi_enable(GD30_SPI);
    spi_gd30ad3344_send_halfword_dma(GD30AD3344_InitStruct_Value);
    delay_1ms(10);
    spi_gd30ad3344_send_halfword_dma(GD30AD3344_InitStruct_Value);
}

/************************************************************
 * Comment  :       读取 GD30AD3344 配置寄存器
 * Parameter:       none
 * Return   :       配置寄存器值
************************************************************/
uint16_t GD30AD3344_Read_ConfigRegister(void)
{
    uint16_t value;

    GD30AD3344_InitStruct.NOP = GD30AD3344_NOP_VALID_NO_UPDATE;
    value = spi_gd30ad3344_send_word_pair_dma(GD30AD3344_InitStruct_Value, GD30AD3344_InitStruct_Value);
    GD30AD3344_InitStruct.NOP = GD30AD3344_NOP_VALID_UPDATE;

    return value;
}

/************************************************************
 * Comment  :       配置外部参考电压
 * Parameter:       none
 * Return   :       验证读取的寄存器值
************************************************************/
uint16_t GD30AD3344_ConfigExternalReference(void)
{
    uint16_t reg_addr = GD30AD3344_EXTREF_REGISTER_ADDR;
    uint16_t proc_cmd;
    uint16_t proc_addr;
    uint16_t proc_data;
    uint16_t read_cmd;
    uint16_t read_addr;
    uint16_t read_value;
    uint16_t write_value;
    uint16_t write_cmd;
    uint16_t write_addr;
    uint16_t write_data;
    uint16_t verify_cmd;
    uint16_t verify_addr;
    uint16_t verify_value;

    spi_gd30ad3344_config_spi(SPI_FRAMESIZE_8BIT);

    proc_data = spi_gd30ad3344_send_word_triple_dma(GD30AD3344_EXTREG_WRITE_CMD,
                                                    GD30AD3344_EXTPROC_REGISTER_ADDR,
                                                    GD30AD3344_EXTPROC_UNLOCK_VALUE,
                                                    &proc_cmd,
                                                    &proc_addr);
    delay_1ms(1);

    read_value = spi_gd30ad3344_send_word_triple_dma(GD30AD3344_EXTREG_READ_CMD,
                                                     reg_addr,
                                                     0x0000U,
                                                     &read_cmd,
                                                     &read_addr);
    delay_1ms(1);

    write_value = read_value | GD30AD3344_EXTREF_ENABLE_BIT;

    write_data = spi_gd30ad3344_send_word_triple_dma(GD30AD3344_EXTREG_WRITE_CMD,
                                                     reg_addr,
                                                     write_value,
                                                     &write_cmd,
                                                     &write_addr);
    delay_1ms(1);

    verify_value = spi_gd30ad3344_send_word_triple_dma(GD30AD3344_EXTREG_READ_CMD,
                                                       reg_addr,
                                                       0x0000U,
                                                       &verify_cmd,
                                                       &verify_addr);

    return verify_value;
}

/************************************************************
 * Comment  :       PGA 枚举值转电压值
 * Parameter:       PGA - PGA 配置枚举
 * Return   :       对应电压值（伏特）
************************************************************/
float ADS118_PGA_SET(GD30AD3344_PGA_TypeDef PGA)
{
    switch (PGA)
    {
    case GD30AD3344_PGA_6V144:
        return 6.144f;
    case GD30AD3344_PGA_4V096:
        return 4.096f;
    case GD30AD3344_PGA_2V048:
        return 2.048f;
    case GD30AD3344_PGA_1V024:
        return 1.024f;
    case GD30AD3344_PGA_0V512:
        return 0.512f;
    case GD30AD3344_PGA_0V256:
        return 0.256f;
    case GD30AD3344_PGA_0V064:
        return 0.064f;
    default:
        return 2.048f;
    }
}

/************************************************************
 * Comment  :       GD30AD3344 ADC 读取通道值
 * Parameter:       CH  - 通道选择
 *                  Ref - PGA 量程配置
 * Return   :       浮点电压值
************************************************************/
float GD30AD3344_AD_Read(GD30AD3344_Channel_TypeDef CH, GD30AD3344_PGA_TypeDef Ref)
{
    uint16_t raw_data;
    float result = 0.0;

    GD30AD3344_InitStruct.MUX = CH;
    GD30AD3344_InitStruct.PGA = Ref;

    raw_data = spi_gd30ad3344_send_halfword_dma(GD30AD3344_InitStruct_Value);
    GD30AD3344_RawCode = raw_data;
    result = (float)((int16_t)raw_data) * ADS118_PGA_SET(Ref) / 32767.0f;

    return (float)result;
}
