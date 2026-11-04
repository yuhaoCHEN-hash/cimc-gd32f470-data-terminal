// 头文件
#ifndef PROTOCOL_FRAME_BUILDER_H
#define PROTOCOL_FRAME_BUILDER_H

#include <stdint.h>
#include <stddef.h>

// Frame field sizes (binary bytes)
#define FRAME_START_SIZE     2
#define FRAME_ID_SIZE        2
#define FRAME_TYPE_SIZE      1
#define FRAME_CMD_SIZE       2
#define FRAME_LEN_SIZE       1
#define FRAME_VER_SIZE       1
#define FRAME_CRC_SIZE       2
#define FRAME_END_SIZE       2
#define FRAME_MIN_BIN_SIZE   (FRAME_START_SIZE + FRAME_ID_SIZE + FRAME_TYPE_SIZE + \
                              FRAME_CMD_SIZE + FRAME_LEN_SIZE + FRAME_VER_SIZE + \
                              FRAME_CRC_SIZE + FRAME_END_SIZE)

#define FRAME_START          0xA5B6
#define FRAME_END            0xB6A5
#define FRAME_PROTOCOL_VER   0x02

#define FRAME_TYPE_CMD       0x01
#define FRAME_TYPE_RESP      0x02
#define FRAME_TYPE_HEARTBEAT 0x05
#define FRAME_TYPE_ERROR     0xFF

#define FRAME_CMD_HEARTBEAT  0x8888
#define FRAME_CMD_BROADCAST  0xFFFF
#define FRAME_CMD_ERROR      0xEEEE

// Max ASCII hex frame buffer size (max content=255 bytes)
#define FRAME_ASCII_MAX      (FRAME_MIN_BIN_SIZE * 2 + 255 * 2 + 4)

// ---- Big-endian byte order helpers ----
static inline uint16_t u16_from_be(const uint8_t b[2])
{
    return (uint16_t)b[0] << 8 | b[1];
}

static inline void u16_to_be(uint8_t dst[2], uint16_t v)
{
    dst[0] = (uint8_t)(v >> 8);
    dst[1] = (uint8_t)v;
}

static inline uint32_t u32_from_be(const uint8_t b[4])
{
    return (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
}

static inline void u32_to_be(uint8_t dst[4], uint32_t v)
{
    dst[0] = (uint8_t)(v >> 24);
    dst[1] = (uint8_t)(v >> 16);
    dst[2] = (uint8_t)(v >> 8);
    dst[3] = (uint8_t)v;
}

// ---- ASCII hex utilities ----
// 函数声明
uint8_t hex_char_to_nibble(char c);
char    nibble_to_hex_char(uint8_t n);
uint8_t hex_to_bytes(const char *hex_str, uint8_t *bytes, uint32_t hex_len);
void    bytes_to_hex(const uint8_t *bytes, uint32_t byte_len, char *hex_str);

// ---- Frame building (returns ASCII hex string length, 0 on error) ----

// Build OK response: type=0x02, content=0xFF
uint32_t frame_build_ok(uint16_t device_id, uint16_t command, char *out_ascii);

// Build data response: type=0x02, content=data
uint32_t frame_build_data(uint16_t device_id, uint16_t command,
                          const uint8_t *data, uint8_t data_len, char *out_ascii);

// Build error response: type=0xFF, cmd=0xEEEE
uint32_t frame_build_error(uint16_t device_id, char *out_ascii);

// Build heartbeat frame: type=0x05, cmd=0x8888
uint32_t frame_build_heartbeat(uint16_t device_id, char *out_ascii);

// Build a raw ASCII hex string from binary frame (for debug or special cases)
uint32_t frame_build_raw(uint16_t device_id, uint8_t frame_type, uint16_t command,
                         const uint8_t *data, uint8_t data_len, char *out_ascii);

#endif
