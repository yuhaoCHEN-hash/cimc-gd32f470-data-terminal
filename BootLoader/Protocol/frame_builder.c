#include "frame_builder.h"
#include "crc16_modbus.h"
#include <string.h>

/* ---------------------------------------------------------------------- */
/* ASCII hex 工具                                                         */
/* ---------------------------------------------------------------------- */

uint8_t hex_char_to_nibble(char c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    return 0;
}

char nibble_to_hex_char(uint8_t n)
{
    n &= 0x0F;
    return (n < 10) ? (char)('0' + n) : (char)('A' + n - 10);
}

uint8_t hex_to_bytes(const char *hex_str, uint8_t *bytes, uint32_t hex_len)
{
    if ((hex_len & 1) != 0)
    {
        return 0;
    }

    for (uint32_t i = 0; i < hex_len; i += 2)
    {
        bytes[i / 2] = (uint8_t)((hex_char_to_nibble(hex_str[i]) << 4) |
                                  hex_char_to_nibble(hex_str[i + 1]));
    }
    return (uint8_t)(hex_len / 2);
}

void bytes_to_hex(const uint8_t *bytes, uint32_t byte_len, char *hex_str)
{
    for (uint32_t i = 0; i < byte_len; i++)
    {
        hex_str[i * 2]     = nibble_to_hex_char(bytes[i] >> 4);
        hex_str[i * 2 + 1] = nibble_to_hex_char(bytes[i] & 0x0F);
    }
    hex_str[byte_len * 2] = '\0';
}

/* ---------------------------------------------------------------------- */
/* 内部：构建二进制帧                                                      */
/* ---------------------------------------------------------------------- */

static uint32_t frame_build_binary(uint16_t device_id, uint8_t frame_type,
                                    uint16_t command, const uint8_t *data,
                                    uint8_t data_len, uint8_t *out_bin)
{
    uint32_t pos = 0;

    out_bin[pos++] = (uint8_t)(FRAME_START >> 8);
    out_bin[pos++] = (uint8_t)(FRAME_START & 0xFF);

    out_bin[pos++] = (uint8_t)(device_id >> 8);
    out_bin[pos++] = (uint8_t)(device_id & 0xFF);

    out_bin[pos++] = frame_type;
    out_bin[pos++] = (uint8_t)(command >> 8);
    out_bin[pos++] = (uint8_t)(command & 0xFF);

    out_bin[pos++] = data_len;
    out_bin[pos++] = FRAME_PROTOCOL_VER;

    if (data_len > 0 && data != NULL)
    {
        memcpy(&out_bin[pos], data, data_len);
        pos += data_len;
    }

    uint16_t crc = crc16_modbus(out_bin, pos);
    out_bin[pos++] = (uint8_t)(crc >> 8);
    out_bin[pos++] = (uint8_t)(crc & 0xFF);

    out_bin[pos++] = (uint8_t)(FRAME_END >> 8);
    out_bin[pos++] = (uint8_t)(FRAME_END & 0xFF);

    return pos;
}

/* ---------------------------------------------------------------------- */
/* 公开 API                                                               */
/* ---------------------------------------------------------------------- */

uint32_t frame_build_ok(uint16_t device_id, uint16_t command, char *out_ascii)
{
    uint8_t ok_content = 0xFF;
    return frame_build_raw(device_id, FRAME_TYPE_RESP, command,
                           &ok_content, 1, out_ascii);
}

uint32_t frame_build_data(uint16_t device_id, uint16_t command,
                           const uint8_t *data, uint8_t data_len, char *out_ascii)
{
    return frame_build_raw(device_id, FRAME_TYPE_RESP, command,
                           data, data_len, out_ascii);
}

uint32_t frame_build_error(uint16_t device_id, char *out_ascii)
{
    return frame_build_raw(device_id, FRAME_TYPE_ERROR, FRAME_CMD_ERROR,
                           NULL, 0, out_ascii);
}

uint32_t frame_build_heartbeat(uint16_t device_id, char *out_ascii)
{
    return frame_build_raw(device_id, FRAME_TYPE_HEARTBEAT, FRAME_CMD_HEARTBEAT,
                           NULL, 0, out_ascii);
}

uint32_t frame_build_raw(uint16_t device_id, uint8_t frame_type, uint16_t command,
                          const uint8_t *data, uint8_t data_len, char *out_ascii)
{
    uint8_t bin[300];
    uint32_t bin_len = frame_build_binary(device_id, frame_type, command,
                                          data, data_len, bin);
    bytes_to_hex(bin, bin_len, out_ascii);
    return bin_len * 2;
}
