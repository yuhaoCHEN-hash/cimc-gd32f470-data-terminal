// 头文件
#include "cimc_f470vet6.h"

// 宏定义
#define WRITE 0x02
#define WRSR 0x01
#define WREN 0x06

#define READ 0x03
#define RDSR 0x05
#define RDID 0x9F
#define SE   0x20
#define BE   0xC7

#define WIP_FLAG   0x01
#define DUMMY_BYTE 0xA5

// 变量声明
extern uint8_t spi1_send_array[ARRAYSIZE];
extern uint8_t spi1_receive_array[ARRAYSIZE];

// 函数定义

/************************************************************
 * Comment  :       初始化 SPI Flash 芯片
 * Parameter:       none
 * Return   :       none
************************************************************/
void spi_flash_init(void)
{
    SPI_FLASH_CS_HIGH();
    spi_enable(SPI_FLASH);
}

/************************************************************
 * Comment  :       擦除指定扇区（4KB）
 * Parameter:       sector_addr - 扇区起始地址
 * Return   :       none
************************************************************/
void spi_flash_sector_erase(uint32_t sector_addr)
{
    spi_flash_write_enable();

    SPI_FLASH_CS_LOW();
    spi_flash_send_byte_dma(SE);
    spi_flash_send_byte_dma((sector_addr & 0xFF0000) >> 16);
    spi_flash_send_byte_dma((sector_addr & 0xFF00) >> 8);
    spi_flash_send_byte_dma(sector_addr & 0xFF);
    SPI_FLASH_CS_HIGH();

    spi_flash_wait_for_write_end();
}

/************************************************************
 * Comment  :       批量擦除整片 Flash
 * Parameter:       none
 * Return   :       none
************************************************************/
void spi_flash_bulk_erase(void)
{
    spi_flash_write_enable();

    SPI_FLASH_CS_LOW();
    spi_flash_send_byte_dma(BE);
    SPI_FLASH_CS_HIGH();

    spi_flash_wait_for_write_end();
}

/************************************************************
 * Comment  :       写一页数据到 Flash（单页内）
 * Parameter:       pbuffer          - 数据缓冲区
 *                  write_addr       - 写入地址
 *                  num_byte_to_write - 写入字节数
 * Return   :       none
************************************************************/
void spi_flash_page_write(uint8_t *pbuffer, uint32_t write_addr, uint16_t num_byte_to_write)
{
    spi_flash_write_enable();

    SPI_FLASH_CS_LOW();
    spi_flash_send_byte_dma(WRITE);
    spi_flash_send_byte_dma((write_addr & 0xFF0000) >> 16);
    spi_flash_send_byte_dma((write_addr & 0xFF00) >> 8);
    spi_flash_send_byte_dma(write_addr & 0xFF);

    while (num_byte_to_write--)
    {
        spi_flash_send_byte_dma(*pbuffer);
        pbuffer++;
    }

    SPI_FLASH_CS_HIGH();
    spi_flash_wait_for_write_end();
}

/************************************************************
 * Comment  :       跨页写数据到 Flash（自动处理页边界）
 * Parameter:       pbuffer          - 数据缓冲区
 *                  write_addr       - 写入地址
 *                  num_byte_to_write - 写入字节数
 * Return   :       none
************************************************************/
void spi_flash_buffer_write(uint8_t *pbuffer, uint32_t write_addr, uint16_t num_byte_to_write)
{
    uint8_t num_of_page = 0, num_of_single = 0, addr = 0, count = 0, temp = 0;

    addr = write_addr % SPI_FLASH_PAGE_SIZE;
    count = SPI_FLASH_PAGE_SIZE - addr;
    num_of_page = num_byte_to_write / SPI_FLASH_PAGE_SIZE;
    num_of_single = num_byte_to_write % SPI_FLASH_PAGE_SIZE;

    if (0 == addr)
    {
        if (0 == num_of_page)
        {
            spi_flash_page_write(pbuffer, write_addr, num_byte_to_write);
        }
        else
        {
            while (num_of_page--)
            {
                spi_flash_page_write(pbuffer, write_addr, SPI_FLASH_PAGE_SIZE);
                write_addr += SPI_FLASH_PAGE_SIZE;
                pbuffer += SPI_FLASH_PAGE_SIZE;
            }
            spi_flash_page_write(pbuffer, write_addr, num_of_single);
        }
    }
    else
    {
        if (0 == num_of_page)
        {
            if (num_of_single > count)
            {
                temp = num_of_single - count;
                spi_flash_page_write(pbuffer, write_addr, count);
                write_addr += count;
                pbuffer += count;
                spi_flash_page_write(pbuffer, write_addr, temp);
            }
            else
            {
                spi_flash_page_write(pbuffer, write_addr, num_byte_to_write);
            }
        }
        else
        {
            num_byte_to_write -= count;
            num_of_page = num_byte_to_write / SPI_FLASH_PAGE_SIZE;
            num_of_single = num_byte_to_write % SPI_FLASH_PAGE_SIZE;

            spi_flash_page_write(pbuffer, write_addr, count);
            write_addr += count;
            pbuffer += count;

            while (num_of_page--)
            {
                spi_flash_page_write(pbuffer, write_addr, SPI_FLASH_PAGE_SIZE);
                write_addr += SPI_FLASH_PAGE_SIZE;
                pbuffer += SPI_FLASH_PAGE_SIZE;
            }

            if (0 != num_of_single)
            {
                spi_flash_page_write(pbuffer, write_addr, num_of_single);
            }
        }
    }
}

/************************************************************
 * Comment  :       从 Flash 读取数据
 * Parameter:       pbuffer          - 数据缓冲区
 *                  read_addr        - 读取地址
 *                  num_byte_to_read  - 读取字节数
 * Return   :       none
************************************************************/
void spi_flash_buffer_read(uint8_t *pbuffer, uint32_t read_addr, uint16_t num_byte_to_read)
{
    SPI_FLASH_CS_LOW();
    spi_flash_send_byte_dma(READ);
    spi_flash_send_byte_dma((read_addr & 0xFF0000) >> 16);
    spi_flash_send_byte_dma((read_addr & 0xFF00) >> 8);
    spi_flash_send_byte_dma(read_addr & 0xFF);

    while (num_byte_to_read--)
    {
        *pbuffer = spi_flash_send_byte_dma(DUMMY_BYTE);
        pbuffer++;
    }

    SPI_FLASH_CS_HIGH();
}

/************************************************************
 * Comment  :       读取 Flash ID
 * Parameter:       none
 * Return   :       24 位 Flash ID
************************************************************/
uint32_t spi_flash_read_id(void)
{
    uint32_t temp = 0, temp0 = 0, temp1 = 0, temp2 = 0;

    SPI_FLASH_CS_LOW();
    spi_flash_send_byte_dma(RDID);
    temp0 = spi_flash_send_byte_dma(DUMMY_BYTE);
    temp1 = spi_flash_send_byte_dma(DUMMY_BYTE);
    temp2 = spi_flash_send_byte_dma(DUMMY_BYTE);
    SPI_FLASH_CS_HIGH();

    temp = (temp0 << 16) | (temp1 << 8) | temp2;
    return temp;
}

/************************************************************
 * Comment  :       开始连续读取序列
 * Parameter:       read_addr - 读取起始地址
 * Return   :       none
************************************************************/
void spi_flash_start_read_sequence(uint32_t read_addr)
{
    SPI_FLASH_CS_LOW();
    spi_flash_send_byte_dma(READ);
    spi_flash_send_byte_dma((read_addr & 0xFF0000) >> 16);
    spi_flash_send_byte_dma((read_addr & 0xFF00) >> 8);
    spi_flash_send_byte_dma(read_addr & 0xFF);
}

/************************************************************
 * Comment  :       写使能
 * Parameter:       none
 * Return   :       none
************************************************************/
void spi_flash_write_enable(void)
{
    SPI_FLASH_CS_LOW();
    spi_flash_send_byte_dma(WREN);
    SPI_FLASH_CS_HIGH();
}

/************************************************************
 * Comment  :       等待写操作完成（轮询 WIP 标志）
 * Parameter:       none
 * Return   :       none
************************************************************/
void spi_flash_wait_for_write_end(void)
{
    uint8_t flash_status = 0;

    SPI_FLASH_CS_LOW();
    spi_flash_send_byte_dma(RDSR);

    do
    {
        flash_status = spi_flash_send_byte_dma(DUMMY_BYTE);
    } while ((flash_status & WIP_FLAG) == 0x01);

    SPI_FLASH_CS_HIGH();
}

/************************************************************
 * Comment  :       通过 DMA 发送并接收一个字节
 * Parameter:       byte - 要发送的字节
 * Return   :       接收到的字节
************************************************************/
uint8_t spi_flash_send_byte_dma(uint8_t byte)
{
    spi1_send_array[0] = byte;

    dma_single_data_parameter_struct dma_init_struct;

    dma_deinit(DMA1, DMA_CH3);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI_FLASH);
    dma_init_struct.memory0_addr        = (uint32_t)spi1_send_array;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_init_struct.number              = 1;
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(DMA1, DMA_CH3, &dma_init_struct);
    dma_channel_subperipheral_select(DMA1, DMA_CH3, DMA_SUBPERI3);

    dma_deinit(DMA1, DMA_CH2);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI_FLASH);
    dma_init_struct.memory0_addr        = (uint32_t)spi1_receive_array;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(DMA1, DMA_CH2, &dma_init_struct);
    dma_channel_subperipheral_select(DMA1, DMA_CH2, DMA_SUBPERI3);

    dma_channel_enable(DMA1, DMA_CH2);
    dma_channel_enable(DMA1, DMA_CH3);

    spi_dma_enable(SPI_FLASH, SPI_DMA_RECEIVE);
    spi_dma_enable(SPI_FLASH, SPI_DMA_TRANSMIT);

    while (RESET == dma_flag_get(DMA1, DMA_CH2, DMA_FLAG_FTF));

    spi_dma_disable(SPI_FLASH, SPI_DMA_RECEIVE);
    spi_dma_disable(SPI_FLASH, SPI_DMA_TRANSMIT);
    dma_channel_disable(DMA1, DMA_CH2);
    dma_channel_disable(DMA1, DMA_CH3);

    dma_flag_clear(DMA1, DMA_CH2, DMA_FLAG_FTF);
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_FTF);

    return spi1_receive_array[0];
}

/************************************************************
 * Comment  :       通过 DMA 发送并接收一个半字（16 位）
 * Parameter:       half_word - 要发送的半字
 * Return   :       接收到的半字
************************************************************/
uint16_t spi_flash_send_halfword_dma(uint16_t half_word)
{
    uint16_t rx_data;

    spi1_send_array[0] = (uint8_t)(half_word >> 8);
    spi1_send_array[1] = (uint8_t)half_word;

    dma_single_data_parameter_struct dma_init_struct;

    dma_deinit(DMA1, DMA_CH3);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI_FLASH);
    dma_init_struct.memory0_addr        = (uint32_t)spi1_send_array;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_init_struct.number              = 2;
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(DMA1, DMA_CH3, &dma_init_struct);
    dma_channel_subperipheral_select(DMA1, DMA_CH3, DMA_SUBPERI3);

    dma_deinit(DMA1, DMA_CH2);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI_FLASH);
    dma_init_struct.memory0_addr        = (uint32_t)spi1_receive_array;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(DMA1, DMA_CH2, &dma_init_struct);
    dma_channel_subperipheral_select(DMA1, DMA_CH2, DMA_SUBPERI3);

    dma_channel_enable(DMA1, DMA_CH2);
    dma_channel_enable(DMA1, DMA_CH3);

    spi_dma_enable(SPI_FLASH, SPI_DMA_RECEIVE);
    spi_dma_enable(SPI_FLASH, SPI_DMA_TRANSMIT);

    while (RESET == dma_flag_get(DMA1, DMA_CH2, DMA_FLAG_FTF));

    spi_dma_disable(SPI_FLASH, SPI_DMA_RECEIVE);
    spi_dma_disable(SPI_FLASH, SPI_DMA_TRANSMIT);
    dma_channel_disable(DMA1, DMA_CH2);
    dma_channel_disable(DMA1, DMA_CH3);

    dma_flag_clear(DMA1, DMA_CH2, DMA_FLAG_FTF);
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_FTF);

    rx_data = (uint16_t)(spi1_receive_array[0] << 8);
    rx_data |= spi1_receive_array[1];

    return rx_data;
}

/************************************************************
 * Comment  :       通过 DMA 发送并接收多个字节
 * Parameter:       tx_buffer - 发送缓冲区
 *                  rx_buffer - 接收缓冲区
 *                  size      - 传输字节数
 * Return   :       none
************************************************************/
void spi_flash_transmit_receive_dma(uint8_t *tx_buffer, uint8_t *rx_buffer, uint16_t size)
{
    if (size > ARRAYSIZE)
    {
        size = ARRAYSIZE;
    }

    for (uint16_t i = 0; i < size; i++)
    {
        spi1_send_array[i] = tx_buffer[i];
    }

    dma_single_data_parameter_struct dma_init_struct;

    dma_deinit(DMA1, DMA_CH3);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI_FLASH);
    dma_init_struct.memory0_addr        = (uint32_t)spi1_send_array;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_init_struct.number              = size;
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(DMA1, DMA_CH3, &dma_init_struct);
    dma_channel_subperipheral_select(DMA1, DMA_CH3, DMA_SUBPERI3);

    dma_deinit(DMA1, DMA_CH2);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(SPI_FLASH);
    dma_init_struct.memory0_addr        = (uint32_t)spi1_receive_array;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(DMA1, DMA_CH2, &dma_init_struct);
    dma_channel_subperipheral_select(DMA1, DMA_CH2, DMA_SUBPERI3);

    dma_channel_enable(DMA1, DMA_CH2);
    dma_channel_enable(DMA1, DMA_CH3);

    spi_dma_enable(SPI_FLASH, SPI_DMA_RECEIVE);
    spi_dma_enable(SPI_FLASH, SPI_DMA_TRANSMIT);

    while (RESET == dma_flag_get(DMA1, DMA_CH2, DMA_FLAG_FTF));

    spi_dma_disable(SPI_FLASH, SPI_DMA_RECEIVE);
    spi_dma_disable(SPI_FLASH, SPI_DMA_TRANSMIT);
    dma_channel_disable(DMA1, DMA_CH2);
    dma_channel_disable(DMA1, DMA_CH3);

    dma_flag_clear(DMA1, DMA_CH2, DMA_FLAG_FTF);
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_FTF);

    for (uint16_t i = 0; i < size; i++)
    {
        rx_buffer[i] = spi1_receive_array[i];
    }
}

/************************************************************
 * Comment  :       等待 DMA 传输完成
 * Parameter:       none
 * Return   :       none
************************************************************/
void spi_flash_wait_for_dma_end(void)
{
    while (RESET == dma_flag_get(DMA1, DMA_CH2, DMA_FLAG_FTF));

    dma_flag_clear(DMA1, DMA_CH2, DMA_FLAG_FTF);
    dma_flag_clear(DMA1, DMA_CH3, DMA_FLAG_FTF);
}

/************************************************************
 * Comment  :       SPI Flash 测试函数
 * Parameter:       none
 * Return   :       none
************************************************************/
void test_spi_flash(void)
{
    uint32_t flash_id;
    uint8_t write_buffer[SPI_FLASH_PAGE_SIZE];
    uint8_t read_buffer[SPI_FLASH_PAGE_SIZE];

    uint32_t test_addr = 0x000000;

    my_printf(rs485_uart, "SPI FLASH Test Start\r\n");

    spi_flash_init();
    my_printf(rs485_uart, "SPI Flash Initialized.\r\n");

    flash_id = spi_flash_read_id();
    my_printf(rs485_uart, "Flash ID: 0x%lX\r\n", flash_id);

    my_printf(rs485_uart, "Erasing sector at address 0x%lX...\r\n", test_addr);
    spi_flash_sector_erase(test_addr);

    my_printf(rs485_uart, "Sector erased.\r\n");

    spi_flash_buffer_read(read_buffer, test_addr, SPI_FLASH_PAGE_SIZE);
    int erased_check_ok = 1;
    for (int i = 0; i < SPI_FLASH_PAGE_SIZE; i++)
    {
        if (read_buffer[i] != 0xFF)
        {
            erased_check_ok = 0;
            break;
        }
    }
    if (erased_check_ok)
    {
        my_printf(rs485_uart, "Erase check PASSED. Sector is all 0xFF.\r\n");
    }
    else
    {
        my_printf(rs485_uart, "Erase check FAILED.\r\n");
    }

    const char *message = "Hello from STM32 to SPI FLASH! Microunion Studio Test - 12345.";
    uint16_t data_len = strlen(message);
    if (data_len >= SPI_FLASH_PAGE_SIZE)
    {
        data_len = SPI_FLASH_PAGE_SIZE - 1;
    }
    memset(write_buffer, 0, SPI_FLASH_PAGE_SIZE);
    memcpy(write_buffer, message, data_len);
    write_buffer[data_len] = '\0';

    my_printf(rs485_uart, "Writing data to address 0x%lX: \"%s\"\r\n", test_addr, write_buffer);
    spi_flash_buffer_write(write_buffer, test_addr, SPI_FLASH_PAGE_SIZE);
    my_printf(rs485_uart, "Data written.\r\n");

    my_printf(rs485_uart, "Reading data from address 0x%lX...\r\n", test_addr);
    memset(read_buffer, 0x00, SPI_FLASH_PAGE_SIZE);
    spi_flash_buffer_read(read_buffer, test_addr, SPI_FLASH_PAGE_SIZE);
    my_printf(rs485_uart, "Data read: \"%.*s\"\r\n", SPI_FLASH_PAGE_SIZE, read_buffer);

    if (memcmp(write_buffer, read_buffer, SPI_FLASH_PAGE_SIZE) == 0)
    {
        my_printf(rs485_uart, "Data VERIFIED! Write and Read successful.\r\n");
    }
    else
    {
        my_printf(rs485_uart, "Data VERIFICATION FAILED!\r\n");
    }

    my_printf(rs485_uart, "SPI FLASH Test End\r\n");
}
