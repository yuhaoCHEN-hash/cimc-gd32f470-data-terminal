// 头文件
#include "frame_parser.h"
#include "frame_builder.h"
#include "crc16_modbus.h"
#include <string.h>

//
// 赛题 4.1 协议帧格式（ASCII 十六进制字符串传输）：
//
//   字段        长度    说明
//   ──────────────────────────────────────
//   起始标志    4字符   固定 "A5B6"（即 0xA5B6 的 ASCII 十六进制）
//   设备 ID     4字符   大端序，如 "0001" = 0x0001
//   帧类型      2字符   "01"(命令) "02"(应答) "05"(心跳) "FF"(错误)
//   命令字      4字符   大端序，如 "0101" = 重启
//   报文长度    2字符   内容 N 的字节数，如 "00" = 0 字节
//   协议版本    2字符   固定 "02"
//   内容        N*2     IEEE754 浮点数或整数的 ASCII 十六进制
//   CRC16       4字符   大端序，对 [起始标志~内容] 计算 CRC-16-Modbus
//   结束标志    4字符   固定 "B6A5"
//
// 例：查询设备重启命令 → A5B600010101000215ABB6A5
//     字段分解： A5B6 | 0001 | 01 | 0101 | 00 | 02 | 15AB | B6A5
//
// 解析器工作方式：逐字节喂入（frame_parser_feed），内部维护环形缓冲区
// 和状态机。当收集到完整的帧并通过 CRC 校验后标记 s_ready=true，
// 调用方通过 frame_parser_get_frame 取出结果。
//

// 宏定义
#define START_ASCII "A5B6"
#define END_ASCII   "B6A5"
#define START_LEN   4
#define END_LEN     4

#define HEADER_ASCII  14
#define HEADER_BIN    7
#define MIN_ASCII     (START_LEN + HEADER_ASCII + END_LEN)

// 变量定义
static char     s_buf[FRAME_ASCII_BUF_SIZE];
static uint32_t s_pos;
static uint32_t s_frame_start;
static uint32_t s_needed;
static bool     s_synced;
static bool     s_ready;
static parse_error_t s_err;

static protocol_frame_t s_frame;

// 函数定义

//
// Comment  :       帧解析器初始化
// Parameter:       none
// Return   :       none
//
void frame_parser_init(void)
{
    s_pos        = 0;
    s_frame_start = 0;
    s_needed     = 0;
    s_synced     = false;
    s_ready      = false;
    s_err        = PARSE_ERR_NONE;
    memset(&s_frame, 0, sizeof(s_frame));
}

//
// Comment  :       帧解析器复位
// Parameter:       none
// Return   :       none
//
void frame_parser_reset(void)
{
    s_pos        = 0;
    s_synced     = false;
    s_needed     = 0;
    s_frame_start = 0;
    s_ready      = false;
}

//
// Comment  :       查询解析器是否正在等待数据
// Parameter:       none
// Return   :       true - 已同步但未完成
//
bool frame_parser_is_pending(void)
{
    return s_synced && !s_ready;
}

//
// Comment  :       在缓冲区中搜索帧起始标志 "A5B6"
// Parameter:       from - 起始搜索偏移
// Return   :       找到的位置，-1 表示未找到
//
static int32_t find_start(uint32_t from)
{
    uint32_t i;
    if (from + START_LEN > s_pos) return -1;
    for (i = from; i + START_LEN <= s_pos; i++)
    {
        if (s_buf[i] == 'A' && s_buf[i+1] == '5' && s_buf[i+2] == 'B' && s_buf[i+3] == '6')
        {
            return (int32_t)i;
        }
    }
    return -1;
}

//
// Comment  :       重新同步：跳过当前帧起始，找下一个 A5B6
// Parameter:       err - 要返回的错误类型
// Return   :       err（调用方直接 return）
//
static parse_error_t resync_after_error(parse_error_t err)
{
    s_frame_start++;
    int32_t next = find_start(s_frame_start);
    if (next < 0)
    {
        frame_parser_reset();
        return err;
    }
    s_frame_start = (uint32_t)next;
    s_needed = 0;
    s_err = err;
    return s_err;
}

//
// Comment  :       向解析器喂入一个字节
// Parameter:       byte - 输入的字节
// Return   :       解析错误类型（PARSE_ERR_NONE 表示无错误）
//
parse_error_t frame_parser_feed(uint8_t byte)
{
    // 如果上一帧尚未被取走（s_ready=true），拒绝接收新字节
    if (s_ready) return PARSE_ERR_NONE;

    // ── 缓冲区防溢出 ──
    // 当缓冲区接近满时（还剩 1 字节空间），丢弃前半段数据，
    // 将后半段移动到缓冲区头部，避免数组越界。这等效于滑动窗口。
    if (s_pos >= FRAME_ASCII_BUF_SIZE - 1)
    {
        uint32_t keep = s_pos / 2;
        memmove(s_buf, &s_buf[s_pos - keep], keep);
        s_pos = keep;
        if (s_synced)
        {
            if (s_frame_start >= (FRAME_ASCII_BUF_SIZE - keep))
                s_frame_start = 0;
            else
                s_synced = false;
        }
    }

    // ── 字节入队 ──
    s_buf[s_pos++] = (char)byte;

    // ── 步骤 1：寻找帧头 "A5B6" ──
    // 未同步时，在最新收到的几个字节中搜索起始标志。
    // 找到后记录 s_frame_start，标记 s_synced=true。
    if (!s_synced)
    {
        int32_t idx = find_start(s_pos > START_LEN ? s_pos - START_LEN : 0);
        if (idx < 0) return PARSE_ERR_NONE;
        s_frame_start = (uint32_t)idx;
        s_synced = true;
    }

    // 从帧头到当前位置的可用字符数
    uint32_t avail = s_pos - s_frame_start;
    if (avail < MIN_ASCII) return PARSE_ERR_NONE;

    // ── 步骤 2：解析报文长度字段 ──
    // 帧格式中，报文长度位于偏移 START_LEN+10~11（即 "A5B6" 后的第 10~11 个字符）。
    // 例如：A5B60001010100 02 15ABB6A5
    //                  ↑↑
    // 长度 "02" = 内容有 2 个十六进制字符 = 1 字节（但内容为空时此处为 "00"）。
    // 计算出整帧需要的总 ASCII 字符数：帧头 + 固定头 + 内容 + CRC + 帧尾
    if (s_needed == 0)
    {
        char len_hex[3];
        len_hex[0] = s_buf[s_frame_start + START_LEN + 10];
        len_hex[1] = s_buf[s_frame_start + START_LEN + 11];
        len_hex[2] = '\0';
        uint8_t content_len;
        hex_to_bytes(len_hex, &content_len, 2);

        s_needed = START_LEN + HEADER_ASCII + (uint32_t)content_len * 2 + FRAME_CRC_SIZE * 2 + END_LEN;

        // 如果需要的总长度超过缓冲区大小，丢弃此帧，重新搜索帧头
        if (s_needed > FRAME_ASCII_BUF_SIZE)
        {
            s_frame_start++;
            s_synced = (find_start(s_frame_start) >= 0);
            s_needed = 0;
            s_err = PARSE_ERR_LENGTH;
            return s_err;
        }
    }

    // 数据还不够整帧，继续等待
    if (avail < s_needed) return PARSE_ERR_NONE;

    // ── 步骤 3：验证帧尾 "B6A5" ──
    // 整帧的最后 4 个字符必须是 B6A5，否则帧格式错误。
    uint32_t end_off = s_frame_start + s_needed - END_LEN;
    if (s_buf[end_off] != 'B' || s_buf[end_off+1] != '6' ||
        s_buf[end_off+2] != 'A' || s_buf[end_off+3] != '5')
    {
        return resync_after_error(PARSE_ERR_LENGTH);
    }

    // ── 步骤 4：ASCII 十六进制 → 二进制 ──
    // 将 ASCII 字符串 "A5B60001..." 还原为二进制字节数组 0xA5 0xB6 0x00 0x01 ...
    // hex_len = 整帧长度 - CRC(4字符) - 帧尾(4字符) = 要转换的字符数
    uint32_t hex_len = s_needed - FRAME_CRC_SIZE * 2 - END_LEN;
    uint8_t bin[300];
    uint8_t bin_len = hex_to_bytes(&s_buf[s_frame_start], bin, hex_len);

    // ── 步骤 5：核对内容长度 ──
    // 从二进制帧中提取内容长度字段（偏移 6），与 hex_to_bytes 实际转换长度比较。
    // 这层校验防止报文长度字段被篡改或误解析。
    uint8_t content_len = bin[FRAME_START_SIZE + FRAME_ID_SIZE + FRAME_TYPE_SIZE +
                              FRAME_CMD_SIZE];
    uint8_t total_expect = FRAME_START_SIZE + FRAME_ID_SIZE + FRAME_TYPE_SIZE +
                           FRAME_CMD_SIZE + FRAME_LEN_SIZE + FRAME_VER_SIZE + content_len;

    if (total_expect != bin_len)
    {
        return resync_after_error(PARSE_ERR_LENGTH);
    }

    // ── 步骤 6：CRC-16-Modbus 校验 ──
    // 对完整的二进制帧（起始标志 ~ 内容）计算 CRC16，
    // 与帧中携带的 CRC 值比较。不一致则丢弃此帧。
    uint16_t crc_calc = crc16_modbus(bin, bin_len);
    uint8_t crc_bin[2];
    hex_to_bytes(&s_buf[s_frame_start + hex_len], crc_bin, FRAME_CRC_SIZE * 2);
    uint16_t crc_recv = u16_from_be(crc_bin);

    if (crc_calc != crc_recv)
    {
        return resync_after_error(PARSE_ERR_CRC);
    }

    // ── 步骤 7：提取字段 ──
    // 所有校验通过，从二进制帧中解析出各字段存入 s_frame 结构体。
    // 帧结构（二进制字节偏移）： [start:0-1][id:2-3][type:4][cmd:5-6][len:7][ver:8][data:9~]
    s_frame.device_id    = u16_from_be(&bin[2]);
    s_frame.frame_type   = bin[4];
    s_frame.command      = u16_from_be(&bin[5]);
    s_frame.data_len     = bin[7];
    s_frame.protocol_ver = bin[8];
    if (s_frame.data_len > 0)
    {
        memcpy(s_frame.data, &bin[9], s_frame.data_len);
    }

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

//
// Comment  :       检查是否有完整帧可读取
// Parameter:       none
// Return   :       true - 有完整帧
//
bool frame_parser_ready(void)
{
    return s_ready;
}

//
// Comment  :       获取已解析的帧数据
// Parameter:       out - 输出帧结构体指针
// Return   :       none
//
void frame_parser_get_frame(protocol_frame_t *out)
{
    if (out != NULL)
    {
        *out = s_frame;
    }
    s_ready = false;
}

//
// Comment  :       获取上次解析错误类型
// Parameter:       none
// Return   :       错误类型枚举
//
parse_error_t frame_parser_get_error_type(void)
{
    return s_err;
}
