#ifndef PROTOCOL_FRAME_PARSER_H
#define PROTOCOL_FRAME_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#define FRAME_MAX_CONTENT    255
#define FRAME_ASCII_BUF_SIZE 600

/* Parsed frame in binary form */
typedef struct {
    uint16_t device_id;
    uint8_t  frame_type;
    uint16_t command;
    uint8_t  data_len;
    uint8_t  protocol_ver;
    uint8_t  data[FRAME_MAX_CONTENT];
} protocol_frame_t;

/* Error codes for error response */
typedef enum {
    PARSE_ERR_NONE          = 0,
    PARSE_ERR_CRC           = 1,
    PARSE_ERR_LENGTH        = 2,
    PARSE_ERR_UNKNOWN_TYPE  = 3,
} parse_error_t;

void            frame_parser_init(void);
parse_error_t   frame_parser_feed(uint8_t byte);
bool            frame_parser_ready(void);
void            frame_parser_get_frame(protocol_frame_t *out);
void            frame_parser_reset(void);
parse_error_t   frame_parser_get_error_type(void);

#endif
