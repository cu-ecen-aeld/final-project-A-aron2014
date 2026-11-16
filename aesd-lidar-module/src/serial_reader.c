#define _GNU_SOURCE

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "ld_lidar.h"
#include "ring_buffer.h"
#include "serial_reader.h"

#define RING_SIZE      16384
#define READ_CHUNK     512
#define POLL_PERIOD_MS 100

struct serial_reader {
    int fd;
    pthread_t thread;
    pthread_mutex_t lock;   
    pthread_cond_t cond;   
    struct ring_buffer rb;
    uint8_t storage[RING_SIZE];
    bool stop;
    bool failed;
    struct serial_reader_stats stats;
};

static void *reader_thread(void *arg)
{
    struct serial_reader *r = arg;
    struct pollfd pfd = { .fd = r->fd, .events = POLLIN };
    uint8_t chunk[READ_CHUNK];
    bool failed = false;

    for (;;) {
        ssize_t n;
        size_t lost;
        int ret;

        pthread_mutex_lock(&r->lock);
        if (r->stop) {
            pthread_mutex_unlock(&r->lock);
            break;
        }
        pthread_mutex_unlock(&r->lock);

        ret = poll(&pfd, 1, POLL_PERIOD_MS);
        if (ret == 0 || (ret < 0 && errno == EINTR))
            continue;
        if (ret < 0) {
            syslog(LOG_ERR, "serial poll failed: %s", strerror(errno));
            failed = true;
            break;
        }

        n = read(r->fd, chunk, sizeof(chunk));
        if (n < 0 && (errno == EAGAIN || errno == EINTR))
            continue;
        if (n < 0) {
            syslog(LOG_ERR, "serial read failed: %s", strerror(errno));
            failed = true;
            break;
        }
        if (n == 0) {
            if (pfd.revents & (POLLHUP | POLLERR)) {
                syslog(LOG_ERR, "serial port hung up");
                failed = true;
                break;
            }
            continue;
        }

        pthread_mutex_lock(&r->lock);
        lost = ring_buffer_write(&r->rb, chunk, (size_t)n);
        r->stats.bytes_read += (uint64_t)n;
        r->stats.overrun_bytes += lost;
        pthread_cond_signal(&r->cond);
        pthread_mutex_unlock(&r->lock);

        if (lost)
            syslog(LOG_WARNING,
                   "serial ring buffer overrun: %zu bytes lost", lost);
    }

    pthread_mutex_lock(&r->lock);
    r->failed = failed;
    pthread_cond_broadcast(&r->cond);  /* wake a consumer blocked in read */
    pthread_mutex_unlock(&r->lock);
    return NULL;
}

struct serial_reader *serial_reader_open(const char *device)
{
    struct serial_reader *r;
    pthread_condattr_t cattr;
    int err;

    r = calloc(1, sizeof(*r));
    if (!r)
        return NULL;

    r->fd = ld_serial_open(device);
    if (r->fd < 0) {
        err = errno;
        syslog(LOG_ERR, "cannot open %s: %s", device, strerror(err));
        free(r);
        errno = err;
        return NULL;
    }

    ring_buffer_init(&r->rb, r->storage, sizeof(r->storage));
    pthread_mutex_init(&r->lock, NULL);

    pthread_condattr_init(&cattr);
    pthread_condattr_setclock(&cattr, CLOCK_MONOTONIC);
    pthread_cond_init(&r->cond, &cattr);
    pthread_condattr_destroy(&cattr);

    err = pthread_create(&r->thread, NULL, reader_thread, r);
    if (err != 0) {
        syslog(LOG_ERR, "cannot start reader thread: %s", strerror(err));
        pthread_cond_destroy(&r->cond);
        pthread_mutex_destroy(&r->lock);
        goto fail_close;
    }

    syslog(LOG_INFO, "reading %s at %d 8N1", device, LD_DEFAULT_BAUD);
    return r;

fail_close:
    close(r->fd);
    free(r);
    errno = err;
    return NULL;
}

ssize_t serial_reader_read(struct serial_reader *r, uint8_t *buf, size_t len,
               int timeout_ms)
{
    struct timespec deadline;
    ssize_t ret;

    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += timeout_ms / 1000;
    deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }

    pthread_mutex_lock(&r->lock);
    while (ring_buffer_count(&r->rb) == 0 && !r->failed && !r->stop) {
        if (pthread_cond_timedwait(&r->cond, &r->lock, &deadline) == ETIMEDOUT)
            break;
    }
    if (ring_buffer_count(&r->rb) > 0)
        ret = (ssize_t)ring_buffer_read(&r->rb, buf, len);
    else if (r->failed)
        ret = -1;
    else
        ret = 0;
    pthread_mutex_unlock(&r->lock);
    return ret;
}

void serial_reader_get_stats(struct serial_reader *r,
                 struct serial_reader_stats *stats)
{
    pthread_mutex_lock(&r->lock);
    *stats = r->stats;
    pthread_mutex_unlock(&r->lock);
}

void serial_reader_close(struct serial_reader *r)
{
    if (!r)
        return;

    pthread_mutex_lock(&r->lock);
    r->stop = true;
    pthread_cond_broadcast(&r->cond);
    pthread_mutex_unlock(&r->lock);

    pthread_join(r->thread, NULL);
    close(r->fd);
    pthread_cond_destroy(&r->cond);
    pthread_mutex_destroy(&r->lock);
    free(r);
}
