// 头文件
#include "ring_buffer.h"
#include <stddef.h>

// 函数定义

//
// Comment  :       初始化环形缓冲区
// Parameter:       r   - 缓冲区对象指针
//                  buf - 底层存储缓冲区
//                  cap - 缓冲区容量
// Return   :       none
//
void ring_buffer_init(ring_buffer_t *r, uint8_t *buf, uint32_t cap)
{
    if (r == NULL)
        return;
    r->buf = buf;
    r->cap = cap;
    r->wp = 0U;
    r->rp = 0U;
    r->cnt = 0U;
}

//
// Comment  :       查询可读数据长度
// Parameter:       r - 缓冲区对象指针
// Return   :       可读字节数
//
uint32_t ring_buffer_available(const ring_buffer_t *r)
{
    if ((r == NULL) || (r->buf == NULL) || (r->cap == 0U))
        return 0U;
    return r->cnt;
}

//
// Comment  :       查询空闲空间长度
// Parameter:       r - 缓冲区对象指针
// Return   :       剩余空闲字节数
//
uint32_t ring_buffer_free(const ring_buffer_t *r)
{
    if ((r == NULL) || (r->buf == NULL) || (r->cap == 0U))
        return 0U;
    return r->cap - r->cnt;
}

//
// Comment  :       查询空闲空间（兼容别名）
// Parameter:       r - 缓冲区对象指针
// Return   :       剩余空闲字节数
//
uint32_t ring_buffer_free_space(const ring_buffer_t *r)
{
    return ring_buffer_free(r);
}

//
// Comment  :       向环形缓冲区写入数据
// Parameter:       r    - 缓冲区对象指针
//                  data - 待写入数据
//                  len  - 数据长度
// Return   :       true  - 写入成功
//                  false - 空间不足
//
bool ring_buffer_write(ring_buffer_t *r, const uint8_t *data, uint32_t len)
{
    if ((r == NULL) || (data == NULL))
        return false;
    if ((r->cap - r->cnt) < len)
        return false;

    for (uint32_t i = 0U; i < len; i++)
    {
        r->buf[r->wp] = data[i];
        r->wp = (r->wp + 1U) % r->cap;
    }
    r->cnt += len;
    return true;
}

//
// Comment  :       查看缓冲区数据（不移除）
// Parameter:       r    - 缓冲区对象指针
//                  data - 输出缓冲区
//                  len  - 期望读取长度
// Return   :       实际读取字节数
//
uint32_t ring_buffer_peek(const ring_buffer_t *r, uint8_t *data, uint32_t len)
{
    if ((r == NULL) || (data == NULL) || (r->buf == NULL) || (r->cap == 0U))
        return 0U;

    uint32_t rd = (len < r->cnt) ? len : r->cnt;
    uint32_t pos = r->rp;
    for (uint32_t i = 0U; i < rd; i++)
    {
        data[i] = r->buf[pos];
        pos = (pos + 1U) % r->cap;
    }
    return rd;
}

//
// Comment  :       从环形缓冲区读取并移除数据
// Parameter:       r    - 缓冲区对象指针
//                  data - 输出缓冲区
//                  len  - 期望读取长度
// Return   :       实际读取字节数
//
uint32_t ring_buffer_read(ring_buffer_t *r, uint8_t *data, uint32_t len)
{
    if ((r == NULL) || (data == NULL) || (r->buf == NULL) || (r->cap == 0U))
        return 0U;

    uint32_t rd = (len < r->cnt) ? len : r->cnt;
    for (uint32_t i = 0U; i < rd; i++)
    {
        data[i] = r->buf[r->rp];
        r->rp = (r->rp + 1U) % r->cap;
    }
    r->cnt -= rd;
    return rd;
}

//
// Comment  :       丢弃指定字节数的数据
// Parameter:       r   - 缓冲区对象指针
//                  len - 丢弃字节数
// Return   :       none
//
void ring_buffer_drop(ring_buffer_t *r, uint32_t len)
{
    if ((r == NULL) || (r->buf == NULL) || (r->cap == 0U))
        return;

    uint32_t drop = (len < r->cnt) ? len : r->cnt;
    r->rp = (r->rp + drop) % r->cap;
    r->cnt -= drop;
}

//
// Comment  :       清空环形缓冲区
// Parameter:       r - 缓冲区对象指针
// Return   :       none
//
void ring_buffer_clear(ring_buffer_t *r)
{
    if (r == NULL)
        return;
    r->wp = 0U;
    r->rp = 0U;
    r->cnt = 0U;
}
