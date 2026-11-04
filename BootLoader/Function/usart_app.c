/* Licence
 * Company: MCUSTUDIO
 * Auther: Ahypnis.
 * Version: V0.10
 * Time: 2026/04/29
 * Note: BootLoader USART1 RS485 — protocol + firmware reception.
 */

#include "CIMC_MCU.h"
#include "frame_parser.h"
#include "frame_builder.h"
#include "bl_cmd_handler.h"
#include "bl_parameters.h"
#include <string.h>

extern uint8_t rxbuffer[OTA_UART_RXBUF_SIZE];

static uint32_t    s_dma_pos = 0U;
static __IO uint16_t tx_count = 0;

/* ---------------------------------------------------------------------- */
/* RS485 printf                                                           */
/* ---------------------------------------------------------------------- */

int my_printf(uint32_t usart_periph, const char *format, ...)
{
    char buffer[512];
    va_list arg;
    int len;

    va_start(arg, format);
    len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    if (len <= 0 || len >= (int)sizeof(buffer))
    {
        return 0;
    }

    if (usart_periph == RS232_RS485_USART)
    {
        RS485_CS_SET(1);
    }

    for (tx_count = 0; tx_count < (uint16_t)len; tx_count++)
    {
        usart_data_transmit(usart_periph, (uint8_t)buffer[tx_count]);
        while (RESET == usart_flag_get(usart_periph, USART_FLAG_TBE))
        {
            ;
        }
    }

    if (usart_periph == RS232_RS485_USART)
    {
        uint32_t tc_timeout = 0xFFFFFUL;
        while ((RESET == usart_flag_get(usart_periph, USART_FLAG_TC)) && (--tc_timeout > 0U))
        {
            ;
        }
        RS485_CS_SET(0);
    }

    return len;
}

/* ---------------------------------------------------------------------- */
/* 裸数据发送                                                             */
/* ---------------------------------------------------------------------- */

static void rs485_send_raw(const char *data, uint32_t len)
{
    if (len == 0 || data == NULL)
    {
        return;
    }

    RS485_CS_SET(1);

    for (uint32_t i = 0; i < len; i++)
    {
        usart_data_transmit(RS232_RS485_USART, (uint8_t)data[i]);
        while (RESET == usart_flag_get(RS232_RS485_USART, USART_FLAG_TBE))
        {
            ;
        }
    }

    uint32_t tc_timeout = 0xFFFFFUL;
    while ((RESET == usart_flag_get(RS232_RS485_USART, USART_FLAG_TC)) && (--tc_timeout > 0U))
    {
        ;
    }
    RS485_CS_SET(0);
}

/* ---------------------------------------------------------------------- */
/* 协议帧处理                                                             */
/* ---------------------------------------------------------------------- */

static void bl_feed_byte(uint8_t byte)
{
    parse_error_t err = frame_parser_feed(byte);

    if (!frame_parser_ready())
    {
        if (err == PARSE_ERR_CRC || err == PARSE_ERR_LENGTH)
        {
            static char err_frame[FRAME_ASCII_MAX];
            uint32_t err_len = frame_build_error(0x0001, err_frame);
            rs485_send_raw(err_frame, err_len);
        }
        return;
    }

    protocol_frame_t req;
    frame_parser_get_frame(&req);

    static char resp[FRAME_ASCII_MAX];
    memset(resp, 0, sizeof(resp));
    uint32_t resp_len = bl_cmd_handler_process(&req, resp);

    if (resp_len > 0 && resp_len >= 4 &&
        resp[0] == 'A' && resp[1] == '5' &&
        resp[2] == 'B' && resp[3] == '6')
    {
        rs485_send_raw(resp, resp_len);

        if (req.command == BL_CMD_UPGRADE_PREPARE)
        {
            s_dma_pos = 0U;
        }

        if (req.command == BL_CMD_UPGRADE_EXECUTE)
        {
            for (volatile uint32_t d = 0; d < 500000U; d++) { __NOP(); }
            if (bl_firmware_validate_and_copy())
            {
                my_printf(rs485_uart, "BL: upgrade success, rebooting...\r\n");
                for (volatile uint32_t d = 0; d < 500000U; d++) { __NOP(); }
            }
            NVIC_SystemReset();
        }
    }
}

/* ---------------------------------------------------------------------- */
/* DMA 轮询                                                               */
/* ---------------------------------------------------------------------- */

static void bl_protocol_poll(void)
{
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

    if (pos == s_dma_pos)
    {
        return;
    }

    uint32_t start_pos = s_dma_pos;

    if (bl_cmd_handler_get_state() == BL_UPGRADE_STATE_RECEIVING)
    {
        if (pos > start_pos)
        {
            bl_firmware_feed(&rxbuffer[start_pos], pos - start_pos);
        }
        else
        {
            bl_firmware_feed(&rxbuffer[start_pos], OTA_UART_RXBUF_SIZE - start_pos);
            if (pos > 0U)
            {
                bl_firmware_feed(rxbuffer, pos);
            }
        }
    }
    else
    {
        if (pos > start_pos)
        {
            for (uint32_t i = 0; i < pos - start_pos; i++)
            {
                bl_feed_byte(rxbuffer[start_pos + i]);
            }
        }
        else
        {
            for (uint32_t i = 0; i < OTA_UART_RXBUF_SIZE - start_pos; i++)
            {
                bl_feed_byte(rxbuffer[start_pos + i]);
            }
            for (uint32_t i = 0; i < pos; i++)
            {
                bl_feed_byte(rxbuffer[i]);
            }
        }
    }

    s_dma_pos = pos;
}

/* ---------------------------------------------------------------------- */
/* 公开 API                                                               */
/* ---------------------------------------------------------------------- */

void rs485_send_frame_pub(const char *data, uint32_t len)
{
    rs485_send_raw(data, len);
}

void uart_task(void)
{
    bl_protocol_poll();
}
