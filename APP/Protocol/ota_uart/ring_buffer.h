// 头文件
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint8_t *buf;
    uint32_t cap;
    uint32_t wp;
    uint32_t rp;
    uint32_t cnt;
} ring_buffer_t;

// 函数声明
void ring_buffer_init(ring_buffer_t *r, uint8_t *buf, uint32_t cap);
uint32_t ring_buffer_available(const ring_buffer_t *r);
uint32_t ring_buffer_free(const ring_buffer_t *r);
uint32_t ring_buffer_free_space(const ring_buffer_t *r);
bool ring_buffer_write(ring_buffer_t *r, const uint8_t *data, uint32_t len);
uint32_t ring_buffer_peek(const ring_buffer_t *r, uint8_t *data, uint32_t len);
uint32_t ring_buffer_read(ring_buffer_t *r, uint8_t *data, uint32_t len);
void ring_buffer_drop(ring_buffer_t *r, uint32_t len);
void ring_buffer_clear(ring_buffer_t *r);

#endif
