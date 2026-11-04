// 头文件
#include "cimc_f470vet6.h"
#include "frame_parser.h"
#include "frame_builder.h"
#include "cmd_handler.h"
#include "param_store.h"

// 变量声明
extern uint8_t rxbuffer[OTA_UART_RXBUF_SIZE];

// 变量定义
uint8_t rs485_rx_buffer[RS485_RXBUF_SIZE];
__IO uint8_t rs485_rx_flag = 0;

__IO uint8_t usart0_rx_flag = 0;
uint8_t usart0_cmd_buffer[256] = {0};

static uint32_t s_ota_dma_pos = 0U;
static uint32_t s_proto_dma_pos = 0U;
static __IO uint16_t tx_count = 0;

static bool s_proto_blocked = false;

// 函数定义

//
// Comment  :       RS485 格式化打印（带方向控制和 TC 等待）
// Parameter:       usart_periph - USART 外设基地址
//                  format       - 格式字符串
//                  ...          - 可变参数
// Return   :       实际发送字符数，失败返回 0
//
int my_printf(uint32_t usart_periph, const char *format, ...)
{
    char buffer[128];
    va_list arg;
    int len;

    if (usart_periph == RS232_RS485_USART && s_proto_blocked)
    {
        return 0;
    }

    va_start(arg, format);
    len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    if (len <= 0 || len >= (int)sizeof(buffer)) return 0;

    RS485_CS_SET(1);
    for (tx_count = 0; tx_count < (uint16_t)len; tx_count++)
    {
        usart_data_transmit(usart_periph, (uint8_t)buffer[tx_count]);
        while (RESET == usart_flag_get(usart_periph, USART_FLAG_TBE));
    }
    {
        uint32_t tc_timeout = 0xFFFFFUL;
        while ((RESET == usart_flag_get(usart_periph, USART_FLAG_TC)) && (--tc_timeout > 0U));
    }
    RS485_CS_SET(0);

    return len;
}

//
// Comment  :       通过 RS485 发送原始缓冲区数据
// Parameter:       data - 数据指针
//                  len  - 数据长度
// Return   :       none
//
static void rs485_send_raw(const char *data, uint32_t len)
{
    uint32_t tc_timeout;

    if (len == 0 || data == NULL) return;

    RS485_CS_SET(1);
    for (uint32_t i = 0; i < len; i++)
    {
        usart_data_transmit(RS232_RS485_USART, (uint8_t)data[i]);
        while (RESET == usart_flag_get(RS232_RS485_USART, USART_FLAG_TBE));
    }
    tc_timeout = 0xFFFFFUL;
    while ((RESET == usart_flag_get(RS232_RS485_USART, USART_FLAG_TC)) && (--tc_timeout > 0U));
    RS485_CS_SET(0);
}

//
// Comment  :       通过 RS485 发送 ASCII 字符串
// Parameter:       str - 以 \0 结尾的字符串
// Return   :       none
//
void rs485_send_string(const char *str)
{
    if (str == NULL || str[0] == '\0') return;
    rs485_send_raw(str, (uint32_t)strlen(str));
}

//
// Comment  :       通过 RS485 发送预构建的协议帧
// Parameter:       data - 帧数据
//                  len  - 帧长度
// Return   :       none
//
void rs485_send_frame_pub(const char *data, uint32_t len)
{
    rs485_send_raw(data, len);
}

//
// Comment  :       复位 OTA 状态
// Parameter:       none
// Return   :       none
//
void ota_reset_state(void)
{
    ota_uart_reset_state();
}

//
// Comment  :       向帧解析器喂字节并处理完成的帧
// Parameter:       byte - 输入的字节
// Return   :       none
//
static void rs485_feed_byte(uint8_t byte)
{
    parse_error_t err = frame_parser_feed(byte);
    if (!frame_parser_ready())
    {
        if (err != PARSE_ERR_NONE)
        {
            static char err_frame[FRAME_ASCII_MAX];
            uint32_t err_len = frame_build_error(cmd_handler_get_device_id(), err_frame);
            rs485_send_raw(err_frame, err_len);
        }
        return;
    }

    protocol_frame_t req;
    frame_parser_get_frame(&req);

    if (req.device_id != cmd_handler_get_device_id() &&
        req.device_id != 0xFFFF)
    {
        return;
    }

    static char resp[FRAME_ASCII_MAX];
    memset(resp, 0, sizeof(resp));
    uint32_t resp_len = cmd_handler_process(&req, resp);

    if (resp_len == 0) return;

    if (resp_len >= 4 && resp[0] == 'A' && resp[1] == '5' &&
        resp[2] == 'B' && resp[3] == '6')
    {
        rs485_send_raw(resp, resp_len);
    }
    else
    {
        rs485_send_string(resp);
    }

    switch (req.command)
    {
    case 0x0101:
        for (volatile uint32_t d = 0; d < 500000U; d++) { __NOP(); }
        NVIC_SystemReset();
        break;

    case 0x01A2:
        if (req.data_len >= 1)
        {
            for (volatile uint32_t d = 0; d < 100000U; d++) { __NOP(); }
            bsp_usart1_ota_init_with_baud(cmd_handler_get_baud_rate_value());
        }
        break;

    case 0x0501:
        param_set_upgrade_pending();
        OLED_ShowStr(0, 2, "Bootloader", 16);
        for (volatile uint32_t d = 0; d < 500000U; d++) { __NOP(); }
        NVIC_SystemReset();
        break;

    case 0x0503:
        for (volatile uint32_t d = 0; d < 500000U; d++) { __NOP(); }
        NVIC_SystemReset();
        break;

    case 0x03AA:
        {
            for (volatile uint32_t d = 0; d < 200000U; d++) { __NOP(); }

            bsp_enter_deepsleep_rtc_wakeup(10);

            s_proto_dma_pos = 0;
            s_ota_dma_pos = 0;
            frame_parser_reset();
            memset((void *)rxbuffer, 0, OTA_UART_RXBUF_SIZE);

            cmd_handler_notify_wakeup();
            rs485_send_string("instrument wakeup\r\n");
        }
        break;

    default:
        break;
    }
}

//
// Comment  :       RS485 协议轮询：从 DMA 缓冲区读取数据并解析
// Parameter:       none
// Return   :       none
//
static void rs485_protocol_poll(void)
{
    uint32_t remaining = dma_transfer_number_get(OTA_UART_DMA, OTA_UART_DMA_CH);
    uint32_t pos;
    static uint32_t pending_no_data_cnt = 0;
    uint32_t start_pos = s_proto_dma_pos;

    if (remaining == 0U) pos = 0U;
    else pos = OTA_UART_RXBUF_SIZE - remaining;

    if (pos != start_pos)
    {
        pending_no_data_cnt = 0;

        if (pos > start_pos)
        {
            for (uint32_t i = 0; i < pos - start_pos; i++)
            {
                rs485_feed_byte(rxbuffer[start_pos + i]);
            }
        }
        else
        {
            for (uint32_t i = 0; i < OTA_UART_RXBUF_SIZE - start_pos; i++)
            {
                rs485_feed_byte(rxbuffer[start_pos + i]);
            }
            for (uint32_t i = 0; i < pos; i++)
            {
                rs485_feed_byte(rxbuffer[i]);
            }
        }
    }

    if (frame_parser_is_pending())
    {
        pending_no_data_cnt++;
        if (pending_no_data_cnt >= 5)
        {
            pending_no_data_cnt = 0;
            frame_parser_reset();
            char err_frame[FRAME_ASCII_MAX];
            uint32_t err_len = frame_build_error(cmd_handler_get_device_id(), err_frame);
            rs485_send_raw(err_frame, err_len);
        }
    }
    else
    {
        pending_no_data_cnt = 0;
    }

    s_proto_dma_pos = pos;
}

//
// Comment  :       USART0 调试任务
// Parameter:       none
// Return   :       none
//
void usart0_task(void)
{
    if (!usart0_rx_flag) return;
    memset(usart0_cmd_buffer, 0, sizeof(usart0_cmd_buffer));
    usart0_rx_flag = 0;
}

//
// Comment  :       USART0 中断处理（IDLE 中断 + DMA 文本命令捕获）
// Parameter:       none
// Return   :       none
//
void USART0_IRQHandler(void)
{
    if (RESET != usart_interrupt_flag_get(USART0, USART_INT_FLAG_IDLE))
    {
        usart_data_receive(USART0);

        if (usart0_rx_flag)
        {
            return;
        }

        static uint32_t s_cmd_pos = 0U;
        uint32_t remaining = dma_transfer_number_get(OTA_UART_DMA, OTA_UART_DMA_CH);
        uint32_t pos;

        if (remaining == 0U)
        {
            pos = 0U;
        }
        else
        {
            pos = OTA_UART_RXBUF_SIZE - remaining;
        }

        {
            uint32_t len = 0U;
            if (pos > s_cmd_pos)
            {
                len = pos - s_cmd_pos;
                if (len > 255U) len = 255U;
                if (len > 0U)
                {
                    memcpy(usart0_cmd_buffer, &rxbuffer[s_cmd_pos], len);
                }
            }
            else if (pos < s_cmd_pos)
            {
                uint32_t tail = OTA_UART_RXBUF_SIZE - s_cmd_pos;
                if (tail > 255U) tail = 255U;
                if (tail > 0U)
                {
                    memcpy(usart0_cmd_buffer, &rxbuffer[s_cmd_pos], tail);
                    len = tail;
                    if (pos > 0U && len < 255U)
                    {
                        uint32_t head = (pos < (255U - len)) ? pos : (255U - len);
                        if (head > 0U)
                        {
                            memcpy(usart0_cmd_buffer + len, rxbuffer, head);
                            len += head;
                        }
                    }
                }
            }
            if (len > 0U)
            {
                usart0_cmd_buffer[len] = '\0';
                usart0_rx_flag = 1U;
            }
        }
        s_cmd_pos = pos;
    }
}

//
// Comment  :       USART1 中断处理（RS485 IDLE 标志清除）
// Parameter:       none
// Return   :       none
//
void USART1_IRQHandler(void)
{
    if (RESET != usart_interrupt_flag_get(USART1, USART_INT_FLAG_IDLE))
    {
        usart_data_receive(USART1);
    }
}

//
// Comment  :       USART2 中断处理（IDLE 标志清除）
// Parameter:       none
// Return   :       none
//
void USART2_IRQHandler(void)
{
    if (RESET != usart_interrupt_flag_get(USART2, USART_INT_FLAG_IDLE))
    {
        usart_data_receive(USART2);
    }
}

//
// Comment  :       主 UART 任务（5ms 周期）
// Parameter:       none
// Return   :       none
//
void uart_task(void)
{
    static bool s_hb_sent = false;
    if (!s_hb_sent)
    {
        s_hb_sent = true;
        char hb[FRAME_ASCII_MAX];
        uint32_t hb_len = frame_build_heartbeat(cmd_handler_get_device_id(), hb);
        rs485_send_raw(hb, hb_len);
    }

    if (ota_uart_is_active())
    {
        ota_uart_task();
        return;
    }

    rs485_protocol_poll();

    cmd_handler_poll();

    usart0_task();
}

//
// Comment  :       RS485 字符串命令任务（向下兼容）
// Parameter:       none
// Return   :       none
//
void rs485_task(void)
{
    if (!rs485_rx_flag) return;

    uint32_t len = strlen((char *)rs485_rx_buffer);
    while (len > 0U && (rs485_rx_buffer[len - 1U] == '\r' ||
                        rs485_rx_buffer[len - 1U] == '\n'))
    {
        rs485_rx_buffer[--len] = '\0';
    }

    if (rs485_rx_buffer[0] != '\0')
    {
        my_printf(RS232_RS485_USART, "ACK: %s\r\n", rs485_rx_buffer);
    }

    memset(rs485_rx_buffer, 0, sizeof(rs485_rx_buffer));
    rs485_rx_flag = 0;
}
