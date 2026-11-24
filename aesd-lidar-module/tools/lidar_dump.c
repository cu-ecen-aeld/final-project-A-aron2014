//lidar_dump exercises the serial reader, ring buffer, and parser without ROS2.
//  lidar_dump /dev/ttyUSB0 [seconds]   specifies the  port, and then prints scan rate each second 
#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <syslog.h>
#include <time.h>

#include "ld_lidar.h"
#include "serial_reader.h"

struct dump {
    ld_parser_t         parser;
    ld_scan_assembler_t assembler;
    ld_scan_t           completed;
    uint32_t            scans;
    uint32_t            dropped_points;
    size_t              last_points;
};

static struct dump d;
static volatile sig_atomic_t stop_requested;

static void on_signal(int sig)
{
    (void)sig;
    stop_requested = 1;
}

static void on_frame(const ld_frame_t *frame, void *ctx)
{
    (void)ctx;
    if (!ld_scan_add_frame(&d.assembler, frame, &d.completed))
        return;
    d.dropped_points += d.completed.dropped;
    if (d.completed.complete) {
        d.scans++;
        d.last_points = d.completed.count;
    }
}

static double now_s(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void print_totals(void)
{
    printf("scans=%u frames=%u crc_errors=%u bytes_discarded=%u dropped_points=%u\n",
           d.scans, d.parser.stats.frames_ok, d.parser.stats.crc_errors,
           d.parser.stats.bytes_discarded, d.dropped_points);
}

static int dump_file(const char *path)
{
    uint8_t buf[4096];
    size_t n;
    FILE *f = fopen(path, "rb");

    if (!f) {
        perror(path);
        return 1;
    }
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        ld_parser_feed(&d.parser, buf, n, on_frame, NULL);
    fclose(f);
    print_totals();
    return 0;
}

static int dump_port(const char *path, double seconds)
{
    struct serial_reader_stats serial;
    struct serial_reader *r;
    struct sigaction sa = { .sa_handler = on_signal };
    uint8_t buf[512];
    uint32_t scans_at_tick = 0;
    double start, tick, elapsed;
    int rc = 0;

    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    r = serial_reader_open(path);
    if (!r) {
        perror(path);
        return 1;
    }

    start = tick = now_s();
    while (!stop_requested) {
        ssize_t n = serial_reader_read(r, buf, sizeof(buf), 100);
        double t = now_s();

        if (n < 0) {
            fprintf(stderr, "serial port failed\n");
            rc = 1;
            break;
        }
        if (n > 0)
            ld_parser_feed(&d.parser, buf, (size_t)n, on_frame, NULL);

        if (t - tick >= 1.0) {
            printf("%6.1f s  %4.1f Hz  %zu points/scan  crc_errors=%u\n",
                   t - start, (double)(d.scans - scans_at_tick) / (t - tick),
                   d.last_points, d.parser.stats.crc_errors);
            fflush(stdout);
            scans_at_tick = d.scans;
            tick = t;
        }
        if (seconds > 0 && t - start >= seconds)
            break;
    }

    elapsed = now_s() - start;
    serial_reader_get_stats(r, &serial);
    serial_reader_close(r);

    print_totals();
    printf("bytes_read=%llu overrun_bytes=%llu elapsed=%.1f s avg_rate=%.2f Hz\n",
           (unsigned long long)serial.bytes_read,
           (unsigned long long)serial.overrun_bytes, elapsed,
           (double)d.scans / elapsed);
    return rc;
}

int main(int argc, char **argv)
{
    struct stat st;
    int rc;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <serial device | capture file> [seconds]\n",
                argv[0]);
        return 2;
    }

    openlog("lidar_dump", LOG_PID, LOG_USER);
    ld_parser_init(&d.parser);
    ld_scan_init(&d.assembler);
    //if the first argument is a character device, treat it as a serial port, otherwise treat it as a capture file
    if (stat(argv[1], &st) == 0 && S_ISCHR(st.st_mode))
        rc = dump_port(argv[1], argc > 2 ? atof(argv[2]) : 0);
    else
        rc = dump_file(argv[1]);

    closelog();
    return rc;
}
