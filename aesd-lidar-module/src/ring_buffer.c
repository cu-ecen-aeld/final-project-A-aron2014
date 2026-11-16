#include "ring_buffer.h"

void ring_buffer_init(struct ring_buffer *rb, uint8_t *storage, size_t size)
{
    rb->data = storage;
    rb->size = size;
    rb->out = 0;
    rb->count = 0;
}

size_t ring_buffer_count(const struct ring_buffer *rb)
{
    return rb->count;
}

size_t ring_buffer_write(struct ring_buffer *rb, const uint8_t *src, size_t len)
{
    size_t lost = 0;
    size_t i;

    for (i = 0; i < len; i++) {
        size_t in = (rb->out + rb->count) % rb->size;

        rb->data[in] = src[i];
        if (rb->count == rb->size) {
            /* Full: the byte just written replaced the oldest one. */
            rb->out = (rb->out + 1) % rb->size;
            lost++;
        } else {
            rb->count++;
        }
    }
    return lost;
}

size_t ring_buffer_read(struct ring_buffer *rb, uint8_t *dst, size_t len)
{
    size_t n = (len < rb->count) ? len : rb->count;
    size_t i;

    for (i = 0; i < n; i++) {
        dst[i] = rb->data[rb->out];
        rb->out = (rb->out + 1) % rb->size;
    }
    rb->count -= n;
    return n;
}
