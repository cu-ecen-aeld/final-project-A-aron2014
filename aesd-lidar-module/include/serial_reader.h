#ifndef SERIAL_READER_H
#define SERIAL_READER_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct serial_reader;

struct serial_reader_stats {
    uint64_t bytes_read;     // bytes received from the port
    uint64_t overrun_bytes;  // bytes overwritten because the consumer fell behind
};

struct serial_reader *serial_reader_open(const char *device);

ssize_t serial_reader_read(struct serial_reader *r, uint8_t *buf, size_t len,
                           int timeout_ms);

void serial_reader_get_stats(struct serial_reader *r,
                             struct serial_reader_stats *stats);

void serial_reader_close(struct serial_reader *r);

#ifdef __cplusplus
}
#endif

#endif
