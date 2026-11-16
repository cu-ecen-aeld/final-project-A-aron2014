#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stddef.h>
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct ring_buffer {
    uint8_t *data;
    size_t   size;   // capacity in bytes 
    size_t   out;    // index of the oldest byte 
    size_t   count;  // bytes currently stored 
};

void ring_buffer_init(struct ring_buffer *rb, uint8_t *storage, size_t size);

size_t ring_buffer_count(const struct ring_buffer *rb);

size_t ring_buffer_write(struct ring_buffer *rb, const uint8_t *src, size_t len);

size_t ring_buffer_read(struct ring_buffer *rb, uint8_t *dst, size_t len);

#ifdef __cplusplus
}
#endif

#endif
