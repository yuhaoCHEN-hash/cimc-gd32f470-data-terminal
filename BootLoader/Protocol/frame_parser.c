#include "frame_parser.h"
#include "frame_builder.h"
#include "crc16_modbus.h"
#include <string.h>

/* ---------------------------------------------------------------------- */
/* 帧边界定                                                               */
/* ---------------------------------------------------------------------- */

#define START_ASCII "A5B6"
#define END_ASCII   "B6A5"
#define START_LEN   4
#define END_LEN     4

#define HEADER_ASCII  14
#define MIN_ASCII     (START_LEN + HEADER_ASCII + END_LEN)

/* ---------------------------------------------------------------------- */
/* 解析器状态                                                             */
/* ---------------------------------------------------------------------- */

static char          s_buf[FRAME_ASCII_BUF_SIZE];
static uint32_t      s_pos;
static uint32_t      s_frame_start;
static uint32_t      s_needed;
static bool          s_synced;
static bool          s_ready;
static parse_error_t s_err;
static protocol_frame_t s_frame;

/* ---------------------------------------------------------------------- */
/* 内部函数                                                               */
/* ---------------------------------------------------------------------- */

static int32_t find_start(uint32_t from)
{
    if (from + START_LEN > s_pos)
    {
        return -1;
    }

    for (uint32_t i = from; i + START_LEN <= s_pos; i++)
    {
        if (s_buf[i] == 'A' && s_buf[i + 1] == '5' &&
            s_buf[i + 2] == 'B' && s_buf[i + 3] == '6')
        {
            return (int32_t)i;
        }
    }
    return -1;
}


void frame_parser_init(void)
{
    s_pos         = 0;
    s_frame_start = 0;
    s_needed      = 0;
    s_synced      = false;
    s_ready       = false;
    s_err         = PARSE_ERR_NONE;
    memset(&s_frame, 0, sizeof(s_frame));
}

void frame_parser_reset(void)
{
    s_pos         = 0;
    s_synced      = false;
    s_needed      = 0;
    s_frame_start = 0;
}

bool frame_parser_is_pending(void)
{
    return s_synced && !s_ready;
}

parse_error_t frame_parser_feed(uint8_t byte)
{
    if (s_ready)
    {
        return PARSE_ERR_NONE;
    }

    /* 缓冲区防溢出 */
    if (s_pos >= FRAME_ASCII_BUF_SIZE - 1)
    {
        uint32_t keep = s_pos / 2;
        memmove(s_buf, &s_buf[s_pos - keep], keep);
        s_pos = keep;
        if (s_synced)
        {
            if (s_frame_start >= (FRAME_ASCII_BUF_SIZE - keep))
            {
                s_frame_start = 0;
            }
            else
            {
                s_synced = false;
            }
        }
    }

    s_buf[s_pos++] = (char)byte;

    /* 搜帧头 */
    if (!s_synced)
    {
        int32_t idx = find_start(s_pos > START_LEN ? s_pos - START_LEN : 0);
        if (idx < 0)
        {
            return PARSE_ERR_NONE;
        }
        s_frame_start = (uint32_t)idx;
        s_synced = true;
    }

    uint32_t avail = s_pos - s_frame_start;
    if (avail < MIN_ASCII)
    {
        return PARSE_ERR_NONE;
    }

    /* 解析内容长度 */
    if (s_needed == 0)
    {
        char len_hex[3];
        len_hex[0] = s_buf[s_frame_start + START_LEN + 10];
        len_hex[1] = s_buf[s_frame_start + START_LEN + 11];
        len_hex[2] = '\0';

        uint8_t content_len;
        hex_to_bytes(len_hex, &content_len, 2);

        s_needed = START_LEN + HEADER_ASCII +
                   (uint32_t)content_len * 2 +
                   FRAME_CRC_SIZE * 2 + END_LEN;

        if (s_needed > FRAME_ASCII_BUF_SIZE)
        {
            s_frame_start++;
            s_synced = (find_start(s_frame_start) >= 0);
            s_needed = 0;
            s_err = PARSE_ERR_LENGTH;
            return s_err;
        }
    }

    if (avail < s_needed)
    {
        return PARSE_ERR_NONE;
    }

    /* ---- 完整帧 ---- */

    /* 校验结束标志 */
    uint32_t end_off = s_frame_start + s_needed - END_LEN;
    if (s_buf[end_off] != 'B' || s_buf[end_off + 1] != '6' ||
        s_buf[end_off + 2] != 'A' || s_buf[end_off + 3] != '5')
    {
        s_frame_start++;
        int32_t next = find_start(s_frame_start);
        if (next < 0) { frame_parser_reset(); return PARSE_ERR_LENGTH; }
        s_frame_start = (uint32_t)next;
        s_needed = 0;
        s_err = PARSE_ERR_LENGTH;
        return s_err;
    }

    /* 转二进制 */
    uint32_t hex_len = s_needed - FRAME_CRC_SIZE * 2 - END_LEN;
    uint8_t bin[300];
    uint8_t bin_len = hex_to_bytes(&s_buf[s_frame_start], bin, hex_len);

    /* 校验长度 */
    uint8_t content_len = bin[FRAME_START_SIZE + FRAME_ID_SIZE +
                               FRAME_TYPE_SIZE + FRAME_CMD_SIZE];
    uint8_t total_expect = FRAME_START_SIZE + FRAME_ID_SIZE +
                           FRAME_TYPE_SIZE + FRAME_CMD_SIZE +
                           FRAME_LEN_SIZE + FRAME_VER_SIZE + content_len;

    if (total_expect != bin_len)
    {
        s_frame_start++;
        int32_t next = find_start(s_frame_start);
        if (next < 0) { frame_parser_reset(); return PARSE_ERR_LENGTH; }
        s_frame_start = (uint32_t)next;
        s_needed = 0;
        s_err = PARSE_ERR_LENGTH;
        return s_err;
    }

    /* CRC16 校验 */
    uint16_t crc_calc = crc16_modbus(bin, bin_len);
    uint8_t crc_bin[2];
    hex_to_bytes(&s_buf[s_frame_start + hex_len], crc_bin, FRAME_CRC_SIZE * 2);
    uint16_t crc_recv = ((uint16_t)crc_bin[0] << 8) | crc_bin[1];

    if (crc_calc != crc_recv)
    {
        s_frame_start++;
        int32_t next = find_start(s_frame_start);
        if (next < 0) { frame_parser_reset(); return PARSE_ERR_CRC; }
        s_frame_start = (uint32_t)next;
        s_needed = 0;
        s_err = PARSE_ERR_CRC;
        return s_err;
    }

    /* 填充结果 */
    s_frame.device_id    = ((uint16_t)bin[2] << 8) | bin[3];
    s_frame.frame_type   = bin[4];
    s_frame.command      = ((uint16_t)bin[5] << 8) | bin[6];
    s_frame.data_len     = bin[7];
    s_frame.protocol_ver = bin[8];
    if (s_frame.data_len > 0)
    {
        memcpy(s_frame.data, &bin[9], s_frame.data_len);
    }

    /* 回收缓冲区 */
    uint32_t consumed = s_frame_start + s_needed;
    if (consumed < s_pos)
    {
        memmove(s_buf, &s_buf[consumed], s_pos - consumed);
    }
    s_pos -= consumed;
    s_frame_start = 0;
    s_needed = 0;
    s_synced = false;
    s_ready = true;
    s_err = PARSE_ERR_NONE;

    return PARSE_ERR_NONE;
}

bool frame_parser_ready(void)
{
    return s_ready;
}

void frame_parser_get_frame(protocol_frame_t *out)
{
    if (out != NULL)
    {
        *out = s_frame;
    }
    s_ready = false;
}

parse_error_t frame_parser_get_error_type(void)
{
    return s_err;
}
