// 头文件
#include "frame_builder.h"
#include "crc16_modbus.h"
#include <string.h>

// 函数定义

//
// Comment  :       十六进制字符转半字节
// Parameter:       c - 十六进制字符（0-9, A-F, a-f）
// Return   :       半字节值（0~15）
//
uint8_t hex_char_to_nibble(char c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    return 0;
}

//
// Comment  :       半字节转十六进制字符
// Parameter:       n - 半字节值
// Return   :       十六进制字符
//
char nibble_to_hex_char(uint8_t n)
{
    n &= 0x0F;
    return (n < 10) ? (char)('0' + n) : (char)('A' + n - 10);
}

//
// Comment  :       十六进制字符串转字节数组
// Parameter:       hex_str - 十六进制字符串
//                  bytes   - 输出字节缓冲区
//                  hex_len - 十六进制字符串长度
// Return   :       转换后的字节数，奇数字长返回 0
//
uint8_t hex_to_bytes(const char *hex_str, uint8_t *bytes, uint32_t hex_len)
{
    uint32_t i;
    if ((hex_len & 1) != 0) return 0;
    for (i = 0; i < hex_len; i += 2)
    {
        bytes[i / 2] = (uint8_t)((hex_char_to_nibble(hex_str[i]) << 4) |
                                  hex_char_to_nibble(hex_str[i + 1]));
    }
    return (uint8_t)(hex_len / 2);
}

//
// Comment  :       字节数组转十六进制字符串
// Parameter:       bytes   - 字节数据
//                  byte_len - 字节长度
//                  hex_str - 输出十六进制字符串
// Return   :       none
//
void bytes_to_hex(const uint8_t *bytes, uint32_t byte_len, char *hex_str)
{
    uint32_t i;
    for (i = 0; i < byte_len; i++)
    {
        hex_str[i * 2]     = nibble_to_hex_char(bytes[i] >> 4);
        hex_str[i * 2 + 1] = nibble_to_hex_char(bytes[i] & 0x0F);
    }
    hex_str[byte_len * 2] = '\0';
}

//
// Comment  :       内部：构建二进制帧
// Parameter:       device_id  - 设备 ID
//                  frame_type - 帧类型
//                  command    - 命令字
//                  data       - 数据负载
//                  data_len   - 数据长度
//                  out_bin    - 输出二进制帧
// Return   :       二进制帧长度
//
static uint32_t frame_build_binary(uint16_t device_id, uint8_t frame_type,
                                   uint16_t command, const uint8_t *data,
                                   uint8_t data_len, uint8_t *out_bin)
{
    uint32_t pos = 0;
    uint32_t crc_range;
    uint16_t crc;

    u16_to_be(&out_bin[pos], FRAME_START);
    pos += 2;

    u16_to_be(&out_bin[pos], device_id);
    pos += 2;

    out_bin[pos++] = frame_type;

    u16_to_be(&out_bin[pos], command);
    pos += 2;

    out_bin[pos++] = data_len;

    out_bin[pos++] = FRAME_PROTOCOL_VER;

    if (data_len > 0 && data != NULL)
    {
        memcpy(&out_bin[pos], data, data_len);
        pos += data_len;
    }

    crc_range = pos;
    crc = crc16_modbus(out_bin, crc_range);
    u16_to_be(&out_bin[pos], crc);
    pos += 2;

    u16_to_be(&out_bin[pos], FRAME_END);
    pos += 2;

    return pos;
}

//
// Comment  :       构建 OK 响应帧
// Parameter:       device_id - 设备 ID
//                  command   - 命令字
//                  out_ascii - 输出 ASCII 十六进制字符串
// Return   :       ASCII 字符串长度
//
uint32_t frame_build_ok(uint16_t device_id, uint16_t command, char *out_ascii)
{
    uint8_t ok_content = 0xFF;
    return frame_build_raw(device_id, FRAME_TYPE_RESP, command,
                           &ok_content, 1, out_ascii);
}

//
// Comment  :       构建数据响应帧
// Parameter:       device_id - 设备 ID
//                  command   - 命令字
//                  data      - 数据负载
//                  data_len  - 数据长度
//                  out_ascii - 输出 ASCII 十六进制字符串
// Return   :       ASCII 字符串长度
//
uint32_t frame_build_data(uint16_t device_id, uint16_t command,
                          const uint8_t *data, uint8_t data_len, char *out_ascii)
{
    return frame_build_raw(device_id, FRAME_TYPE_RESP, command,
                           data, data_len, out_ascii);
}

//
// Comment  :       构建错误响应帧
// Parameter:       device_id - 设备 ID
//                  out_ascii - 输出 ASCII 十六进制字符串
// Return   :       ASCII 字符串长度
//
uint32_t frame_build_error(uint16_t device_id, char *out_ascii)
{
    return frame_build_raw(device_id, FRAME_TYPE_ERROR, FRAME_CMD_ERROR,
                           NULL, 0, out_ascii);
}

//
// Comment  :       构建心跳帧
// Parameter:       device_id - 设备 ID
//                  out_ascii - 输出 ASCII 十六进制字符串
// Return   :       ASCII 字符串长度
//
uint32_t frame_build_heartbeat(uint16_t device_id, char *out_ascii)
{
    return frame_build_raw(device_id, FRAME_TYPE_HEARTBEAT, FRAME_CMD_HEARTBEAT,
                           NULL, 0, out_ascii);
}

//
// Comment  :       构建原始 ASCII 十六进制帧
// Parameter:       device_id  - 设备 ID
//                  frame_type - 帧类型
//                  command    - 命令字
//                  data       - 数据负载
//                  data_len   - 数据长度
//                  out_ascii  - 输出 ASCII 十六进制字符串
// Return   :       ASCII 字符串长度
//
uint32_t frame_build_raw(uint16_t device_id, uint8_t frame_type, uint16_t command,
                         const uint8_t *data, uint8_t data_len, char *out_ascii)
{
    uint8_t bin[300];
    uint32_t bin_len;

    bin_len = frame_build_binary(device_id, frame_type, command, data, data_len, bin);
    bytes_to_hex(bin, bin_len, out_ascii);
    return bin_len * 2;
}
