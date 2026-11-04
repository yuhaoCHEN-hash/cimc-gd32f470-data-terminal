// 头文件
#ifndef PROTOCOL_FRAME_PARSER_H
#define PROTOCOL_FRAME_PARSER_H

#include <stdint.h>
#include <stdbool.h>

// 宏定义
#define FRAME_MAX_CONTENT    255
#define FRAME_ASCII_BUF_SIZE 600

// 已解析的帧结构体（二进制形式）
typedef struct
{
    uint16_t device_id;
    uint8_t  frame_type;
    uint16_t command;
    uint8_t  data_len;
    uint8_t  protocol_ver;
    uint8_t  data[FRAME_MAX_CONTENT];
} protocol_frame_t;

// 解析错误类型
typedef enum
{
    PARSE_ERR_NONE          = 0,
    PARSE_ERR_CRC           = 1,
    PARSE_ERR_LENGTH        = 2,
    PARSE_ERR_UNKNOWN_TYPE  = 3,
} parse_error_t;

// 函数声明
void            frame_parser_init(void);
parse_error_t   frame_parser_feed(uint8_t byte);
bool            frame_parser_ready(void);
void            frame_parser_get_frame(protocol_frame_t *out);
void            frame_parser_reset(void);
bool            frame_parser_is_pending(void);
parse_error_t   frame_parser_get_error_type(void);

#endif
