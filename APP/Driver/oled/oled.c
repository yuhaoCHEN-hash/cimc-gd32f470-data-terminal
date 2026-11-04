// 头文件
#include "oled.h"
#include "oledfont.h"
#include "gd32f4xx_i2c.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_dma.h"
#include "perf_counter.h"

// 变量声明
extern uint8_t oled_cmd_buf[2];
extern uint8_t oled_data_buf[2];

// 宏定义
#define OLED_I2C_WAIT_TIMEOUT 100000U
#define OLED_I2C_ADDR_WRITE 0x78U

// 变量定义
uint8_t s_oled_available = 1U;

// 函数定义

/************************************************************
 * Comment  :       I2C 总线恢复：释放被拉低的 SDA
 * Parameter:       none
 * Return   :       none
************************************************************/
static void I2C_Bus_Reset(void)
{
    uint8_t i;

    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_8 | GPIO_PIN_9);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_8 | GPIO_PIN_9);

    gpio_bit_set(GPIOB, GPIO_PIN_8 | GPIO_PIN_9);
    delay_ms(10);

    for (i = 0; i < 9; i++)
    {
        gpio_bit_reset(GPIOB, GPIO_PIN_8);
        delay_ms(5);
        gpio_bit_set(GPIOB, GPIO_PIN_8);
        delay_ms(5);
    }

    gpio_bit_set(GPIOB, GPIO_PIN_8);
    gpio_bit_reset(GPIOB, GPIO_PIN_9);
    delay_ms(5);
    gpio_bit_set(GPIOB, GPIO_PIN_9);
    delay_ms(5);

    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_9);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_9);
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_8);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_8);

    i2c_deinit(I2C0);
    i2c_clock_config(I2C0, 400000, I2C_DTCY_2);
    i2c_mode_addr_config(I2C0, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, 0x72);
    i2c_enable(I2C0);
    i2c_ack_config(I2C0, I2C_ACK_ENABLE);

    delay_ms(10);
}

/************************************************************
 * Comment  :       等待 I2C 标志位置位，超时返回
 * Parameter:       i2c_periph - I2C 外设基地址
 *                  flag      - 等待的标志位
 *                  timeout   - 超时计数值
 * Return   :       1 - 标志位置位，0 - 超时
************************************************************/
static uint8_t oled_wait_i2c_flag_set(uint32_t i2c_periph, i2c_flag_enum flag, uint32_t timeout)
{
    while (timeout--)
    {
        if (SET == i2c_flag_get(i2c_periph, flag))
        {
            return 1U;
        }
    }
    return 0U;
}

/************************************************************
 * Comment  :       等待 I2C STOP 条件清除
 * Parameter:       i2c_periph - I2C 外设基地址
 *                  timeout   - 超时计数值
 * Return   :       1 - 清除成功，0 - 超时
************************************************************/
static uint8_t oled_wait_i2c_stop_clear(uint32_t i2c_periph, uint32_t timeout)
{
    while (timeout--)
    {
        if (0U == (I2C_CTL0(i2c_periph) & I2C_CTL0_STOP))
        {
            return 1U;
        }
    }
    return 0U;
}

/************************************************************
 * Comment  :       等待 DMA 传输完成标志
 * Parameter:       dma_periph - DMA 外设
 *                  channel   - DMA 通道
 *                  timeout   - 超时计数值
 * Return   :       1 - 完成，0 - 超时
************************************************************/
static uint8_t oled_wait_dma_ftf(uint32_t dma_periph, dma_channel_enum channel, uint32_t timeout)
{
    while (timeout--)
    {
        if (SET == dma_flag_get(dma_periph, channel, DMA_FLAG_FTF))
        {
            return 1U;
        }
    }
    return 0U;
}

/************************************************************
 * Comment  :       等待 I2C ADDSEND 或检测 NACK
 * Parameter:       timeout - 超时计数值
 * Return   :       1 - ADDSEND，0 - NACK 或超时
************************************************************/
static uint8_t oled_wait_addsend_or_nack(uint32_t timeout)
{
    while (timeout--)
    {
        if (SET == i2c_flag_get(I2C0, I2C_FLAG_ADDSEND))
        {
            return 1U;
        }
        if (SET == i2c_flag_get(I2C0, I2C_FLAG_AERR))
        {
            i2c_flag_clear(I2C0, I2C_FLAG_AERR);
            return 0U;
        }
    }
    return 0U;
}

uint8_t initcmd1[] =
{
    0xAE,       // display off
    0xD5, 0x80, // Set Display Clock Divide Ratio/Oscillator Frequency
    0xA8, 0x1F, // set multiplex Ratio
    0xD3, 0x00, // display offset
    0x40,       // set display start line
    0x8d, 0x14, // set charge pump
    0xa1,       // set segment remap
    0xc8,       // set com output scan direction
    0xda, 0x00, // set com pins hardware configuration
    0x81, 0x80, // set contrast control
    0xd9, 0x1f, // set pre-charge period
    0xdb, 0x40, // set vcom deselect level
    0xa4,       // Set Entire Display On/Off
    0xaf,       // set display on
};

/************************************************************
 * Comment  :       向 OLED 写命令字节
 * Parameter:       cmd - 命令字节
 * Return   :       none
************************************************************/
void OLED_Write_cmd(uint8_t cmd)
{
    uint32_t timeout = 10000;

    if (0U == s_oled_available)
    {
        return;
    }

    oled_cmd_buf[0] = 0x00;
    oled_cmd_buf[1] = cmd;

    if (i2c_flag_get(I2C0, I2C_FLAG_I2CBSY))
    {
        I2C_Bus_Reset();
    }

    while (i2c_flag_get(I2C0, I2C_FLAG_I2CBSY) && (--timeout > 0))
    {
        delay_ms(1);
    }

    if (timeout == 0)
    {
        I2C_Bus_Reset();
        timeout = 10000;
        while (i2c_flag_get(I2C0, I2C_FLAG_I2CBSY) && (--timeout > 0))
        {
            delay_ms(1);
        }
        if (timeout == 0)
        {
            return;
        }
    }

    i2c_start_on_bus(I2C0);
    if (!oled_wait_i2c_flag_set(I2C0, I2C_FLAG_SBSEND, OLED_I2C_WAIT_TIMEOUT))
    {
        I2C_Bus_Reset();
        s_oled_available = 0U;
        return;
    }

    i2c_master_addressing(I2C0, OLED_I2C_ADDR_WRITE, I2C_TRANSMITTER);
    if (!oled_wait_addsend_or_nack(OLED_I2C_WAIT_TIMEOUT))
    {
        i2c_stop_on_bus(I2C0);
        (void)oled_wait_i2c_stop_clear(I2C0, OLED_I2C_WAIT_TIMEOUT / 10U);
        I2C_Bus_Reset();
        s_oled_available = 0U;
        return;
    }

    i2c_flag_clear(I2C0, I2C_FLAG_ADDSEND);
    if (!oled_wait_i2c_flag_set(I2C0, I2C_FLAG_TBE, OLED_I2C_WAIT_TIMEOUT))
    {
        I2C_Bus_Reset();
        s_oled_available = 0U;
        return;
    }

    dma_memory_address_config(DMA0, DMA_CH6, DMA_MEMORY_0, (uint32_t)oled_cmd_buf);
    dma_transfer_number_config(DMA0, DMA_CH6, 2);
    i2c_dma_config(I2C0, I2C_DMA_ON);
    dma_channel_enable(DMA0, DMA_CH6);

    if (!oled_wait_dma_ftf(DMA0, DMA_CH6, OLED_I2C_WAIT_TIMEOUT))
    {
        dma_channel_disable(DMA0, DMA_CH6);
        i2c_dma_config(I2C0, I2C_DMA_OFF);
        I2C_Bus_Reset();
        s_oled_available = 0U;
        return;
    }

    dma_flag_clear(DMA0, DMA_CH6, DMA_FLAG_FTF);
    dma_channel_disable(DMA0, DMA_CH6);
    i2c_dma_config(I2C0, I2C_DMA_OFF);
    i2c_stop_on_bus(I2C0);

    if (!oled_wait_i2c_stop_clear(I2C0, OLED_I2C_WAIT_TIMEOUT))
    {
        I2C_Bus_Reset();
        s_oled_available = 0U;
        return;
    }
}

/************************************************************
 * Comment  :       向 OLED 写数据字节
 * Parameter:       data - 数据字节
 * Return   :       none
************************************************************/
void OLED_Write_data(uint8_t data)
{
    uint32_t timeout = 10000;

    if (0U == s_oled_available)
    {
        return;
    }

    oled_data_buf[0] = 0x40;
    oled_data_buf[1] = data;

    if (i2c_flag_get(I2C0, I2C_FLAG_I2CBSY))
    {
        I2C_Bus_Reset();
    }

    while (i2c_flag_get(I2C0, I2C_FLAG_I2CBSY) && (--timeout > 0))
    {
        delay_ms(1);
    }

    if (timeout == 0)
    {
        I2C_Bus_Reset();
        timeout = 10000;
        while (i2c_flag_get(I2C0, I2C_FLAG_I2CBSY) && (--timeout > 0))
        {
            delay_ms(1);
        }
        if (timeout == 0)
        {
            return;
        }
    }

    i2c_start_on_bus(I2C0);
    if (!oled_wait_i2c_flag_set(I2C0, I2C_FLAG_SBSEND, OLED_I2C_WAIT_TIMEOUT))
    {
        I2C_Bus_Reset();
        s_oled_available = 0U;
        return;
    }

    i2c_master_addressing(I2C0, OLED_I2C_ADDR_WRITE, I2C_TRANSMITTER);
    if (!oled_wait_addsend_or_nack(OLED_I2C_WAIT_TIMEOUT))
    {
        i2c_stop_on_bus(I2C0);
        (void)oled_wait_i2c_stop_clear(I2C0, OLED_I2C_WAIT_TIMEOUT / 10U);
        I2C_Bus_Reset();
        s_oled_available = 0U;
        return;
    }

    i2c_flag_clear(I2C0, I2C_FLAG_ADDSEND);
    if (!oled_wait_i2c_flag_set(I2C0, I2C_FLAG_TBE, OLED_I2C_WAIT_TIMEOUT))
    {
        I2C_Bus_Reset();
        s_oled_available = 0U;
        return;
    }

    dma_memory_address_config(DMA0, DMA_CH6, DMA_MEMORY_0, (uint32_t)oled_data_buf);
    dma_transfer_number_config(DMA0, DMA_CH6, 2);
    i2c_dma_config(I2C0, I2C_DMA_ON);
    dma_channel_enable(DMA0, DMA_CH6);

    if (!oled_wait_dma_ftf(DMA0, DMA_CH6, OLED_I2C_WAIT_TIMEOUT))
    {
        dma_channel_disable(DMA0, DMA_CH6);
        i2c_dma_config(I2C0, I2C_DMA_OFF);
        I2C_Bus_Reset();
        s_oled_available = 0U;
        return;
    }

    dma_flag_clear(DMA0, DMA_CH6, DMA_FLAG_FTF);
    dma_channel_disable(DMA0, DMA_CH6);
    i2c_dma_config(I2C0, I2C_DMA_OFF);
    i2c_stop_on_bus(I2C0);

    if (!oled_wait_i2c_stop_clear(I2C0, OLED_I2C_WAIT_TIMEOUT))
    {
        I2C_Bus_Reset();
        s_oled_available = 0U;
        return;
    }
}

/************************************************************
 * Comment  :       显示图片
 * Parameter:       x0, y0 - 起始坐标
 *                  x1, y1 - 结束坐标
 *                  BMP    - 图片数据
 * Return   :       none
************************************************************/
void OLED_ShowPic(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t BMP[])
{
    uint16_t i = 0;
    uint8_t x, y;
    for (y = y0; y < y1; y++)
    {
        OLED_Set_Position(x0, y);
        for (x = x0; x < x1; x++)
        {
            OLED_Write_data(BMP[i++]);
        }
    }
}

/************************************************************
 * Comment  :       显示 16x16 汉字
 * Parameter:       x, y - 坐标
 *                  no   - 汉字索引
 * Return   :       none
************************************************************/
void OLED_ShowHanzi(uint8_t x, uint8_t y, uint8_t no)
{
    uint8_t t, adder = 0;
    OLED_Set_Position(x, y);
    for (t = 0; t < 16; t++)
    {
        OLED_Write_data(Hzk[2 * no][t]);
        adder += 1;
    }
    OLED_Set_Position(x, y + 1);
    for (t = 0; t < 16; t++)
    {
        OLED_Write_data(Hzk[2 * no + 1][t]);
        adder += 1;
    }
}

/************************************************************
 * Comment  :       显示 32x32 汉字
 * Parameter:       x, y - 坐标
 *                  n    - 汉字索引
 * Return   :       none
************************************************************/
void OLED_ShowHzbig(uint8_t x, uint8_t y, uint8_t n)
{
    uint8_t t, adder = 0;
    OLED_Set_Position(x, y);
    for (t = 0; t < 32; t++)
    {
        OLED_Write_data(Hzb[4 * n][t]);
        adder += 1;
    }
    OLED_Set_Position(x, y + 1);
    for (t = 0; t < 32; t++)
    {
        OLED_Write_data(Hzb[4 * n + 1][t]);
        adder += 1;
    }

    OLED_Set_Position(x, y + 2);
    for (t = 0; t < 32; t++)
    {
        OLED_Write_data(Hzb[4 * n + 2][t]);
        adder += 1;
    }
    OLED_Set_Position(x, y + 3);
    for (t = 0; t < 32; t++)
    {
        OLED_Write_data(Hzb[4 * n + 3][t]);
        adder += 1;
    }
}

/************************************************************
 * Comment  :       显示浮点数
 * Parameter:       x, y     - 坐标
 *                  num      - 浮点数值
 *                  accuracy - 小数位数
 *                  fontsize - 字体大小（8 或 16）
 * Return   :       none
************************************************************/
void OLED_ShowFloat(uint8_t x, uint8_t y, float num, uint8_t accuracy, uint8_t fontsize)
{
    uint8_t i = 0;
    uint8_t j = 0;
    uint8_t t = 0;
    uint8_t temp = 0;
    uint16_t numel = 0;
    uint32_t integer = 0;
    float decimals = 0;

    if (num < 0)
    {
        OLED_ShowChar(x, y, '-', fontsize);
        num = 0 - num;
        i++;
    }

    integer = (uint32_t)num;
    decimals = num - integer;

    if (integer)
    {
        numel = integer;

        while (numel)
        {
            numel /= 10;
            j++;
        }
        i += (j - 1);
        for (temp = 0; temp < j; temp++)
        {
            OLED_ShowChar(x + 8 * (i - temp), y, integer % 10 + '0', fontsize);
            integer /= 10;
        }
    }
    else
    {
        OLED_ShowChar(x + 8 * i, y, temp + '0', fontsize);
    }
    i++;
    if (accuracy)
    {
        OLED_ShowChar(x + 8 * i, y, '.', fontsize);

        i++;
        for (t = 0; t < accuracy; t++)
        {
            decimals *= 10;
            temp = (uint8_t)decimals;
            OLED_ShowChar(x + 8 * (i + t), y, temp + '0', fontsize);
            decimals -= temp;
        }
    }
}

/************************************************************
 * Comment  :       幂运算（整数）
 * Parameter:       a - 底数
 *                  n - 指数
 * Return   :       计算结果
************************************************************/
static uint32_t OLED_Pow(uint8_t a, uint8_t n)
{
    uint32_t result = 1;
    while (n--)
    {
        result *= a;
    }
    return result;
}

/************************************************************
 * Comment  :       显示无符号整数
 * Parameter:       x, y     - 坐标
 *                  num      - 数值
 *                  length   - 显示位数
 *                  fontsize - 字体大小（8 或 16）
 * Return   :       none
************************************************************/
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t length, uint8_t fontsize)
{
    uint8_t t, temp;
    uint8_t enshow = 0;
    for (t = 0; t < length; t++)
    {
        temp = (num / OLED_Pow(10, length - t - 1)) % 10;
        if (enshow == 0 && t < (length - 1))
        {
            if (temp == 0)
            {
                OLED_ShowChar(x + (fontsize / 2) * t, y, ' ', fontsize);
                continue;
            }
            else
                enshow = 1;
        }
        OLED_ShowChar(x + (fontsize / 2) * t, y, temp + '0', fontsize);
    }
}

/************************************************************
 * Comment  :       显示字符串
 * Parameter:       x, y     - 坐标
 *                  ch       - 字符串指针
 *                  fontsize - 字体大小（8 或 16）
 * Return   :       none
************************************************************/
void OLED_ShowStr(uint8_t x, uint8_t y, char *ch, uint8_t fontsize)
{
    uint8_t j = 0;
    while (ch[j] != '\0')
    {
        OLED_ShowChar(x, y, ch[j], fontsize);
        x += 8;
        if (x > 120)
        {
            x = 0;
            y += 2;
        }
        j++;
    }
}

/************************************************************
 * Comment  :       显示一个字符
 * Parameter:       x, y     - 坐标
 *                  ch       - 字符 ASCII 码
 *                  fontsize - 字体大小（8 或 16）
 * Return   :       none
************************************************************/
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t ch, uint8_t fontsize)
{
    uint8_t c = 0, i = 0;
    c = ch - ' ';

    if (x > 127)
    {
        x = 0;
        y++;
    }

    if (fontsize == 16)
    {
        OLED_Set_Position(x, y);
        for (i = 0; i < 8; i++)
        {
            OLED_Write_data(F8X16[c * 16 + i]);
        }
        OLED_Set_Position(x, y + 1);
        for (i = 0; i < 8; i++)
        {
            OLED_Write_data(F8X16[c * 16 + i + 8]);
        }
    }
    else
    {
        OLED_Set_Position(x, y);
        for (i = 0; i < 6; i++)
        {
            OLED_Write_data(F6X8[c][i]);
        }
    }
}

/************************************************************
 * Comment  :       全屏填充
 * Parameter:       none
 * Return   :       none
************************************************************/
void OLED_Allfill(void)
{
    uint8_t i, j;
    for (i = 0; i < 4; i++)
    {
        OLED_Write_cmd(0xb0 + i);
        OLED_Write_cmd(0x00);
        OLED_Write_cmd(0x10);
        for (j = 0; j < 128; j++)
        {
            OLED_Write_data(0xFF);
        }
    }
}

/************************************************************
 * Comment  :       设置光标位置
 * Parameter:       x - X 坐标
 *                  y - Y 坐标
 * Return   :       none
************************************************************/
void OLED_Set_Position(uint8_t x, uint8_t y)
{
    OLED_Write_cmd(0xb0 + y);
    OLED_Write_cmd(((x & 0xf0) >> 4) | 0x10);
    OLED_Write_cmd((x & 0x0f) | 0x00);
}

/************************************************************
 * Comment  :       清屏
 * Parameter:       none
 * Return   :       none
************************************************************/
void OLED_Clear(void)
{
    uint8_t i, n;
    for (i = 0; i < 4; i++)
    {
        OLED_Write_cmd(0xb0 + i);
        OLED_Write_cmd(0x00);
        OLED_Write_cmd(0x10);
        for (n = 0; n < 128; n++)
        {
            OLED_Write_data(0);
        }
    }
}

/************************************************************
 * Comment  :       打开 OLED 显示
 * Parameter:       none
 * Return   :       none
************************************************************/
void OLED_Display_On(void)
{
    OLED_Write_cmd(0x8D);
    OLED_Write_cmd(0x14);
    OLED_Write_cmd(0xAF);
}

/************************************************************
 * Comment  :       关闭 OLED 显示
 * Parameter:       none
 * Return   :       none
************************************************************/
void OLED_Display_Off(void)
{
    OLED_Write_cmd(0x8D);
    OLED_Write_cmd(0x10);
    OLED_Write_cmd(0xAF);
}

/************************************************************
 * Comment  :       OLED 初始化
 * Parameter:       none
 * Return   :       none
************************************************************/
void OLED_Init(void)
{
    delay_ms(100);
    s_oled_available = 1U;

    {
        uint8_t i;
        for (i = 0; i < sizeof(initcmd1); i++)
        {
            OLED_Write_cmd(initcmd1[i]);
            if (0U == s_oled_available)
            {
                return;
            }
        }
    }

    OLED_Clear();
    OLED_Set_Position(0, 0);
}
