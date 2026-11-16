
#ifndef LD_LIDAR_H
#define LD_LIDAR_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define LD_HEADER            0x54u
#define LD_VERLEN            0x2Cu
#define LD_POINTS_PER_FRAME  12u
#define LD_FRAME_LEN         47u
#define LD_ANGLE_FULL_CDEG   36000u
#define LD_ANGLE_INVALID     0xFFFFu
#define LD_DEFAULT_BAUD      230400

#define LD_OFF_SPEED         2u
#define LD_OFF_START_ANGLE   4u
#define LD_OFF_POINTS        6u
#define LD_OFF_END_ANGLE     42u
#define LD_OFF_TIMESTAMP     44u
#define LD_OFF_CRC           46u

typedef struct {
    uint16_t distance_mm;
    uint8_t  intensity;
} ld_point_t;

typedef struct {
    uint16_t   speed_dps;         // rotation speed, deg/s
    uint16_t   start_angle_cdeg;  // 0.01 deg
    uint16_t   end_angle_cdeg;    // 0.01 deg
    uint16_t   timestamp_ms;      // wraps at 30000
    ld_point_t points[LD_POINTS_PER_FRAME];
} ld_frame_t;

typedef enum {
    LD_NEED_MORE = 0,   // byte consumed, no frame yet
    LD_FRAME_OK  = 1,   // *out holds a freshly decoded, CRC-valid frame
    LD_ERR_CRC   = -1,  // a candidate frame failed CRC; parser resynced
} ld_status_t;

typedef struct {
    uint32_t frames_ok;
    uint32_t crc_errors;       
    uint32_t bytes_discarded;  
} ld_stats_t;

typedef struct {
    uint8_t    buf[LD_FRAME_LEN];
    size_t     len;            
    ld_stats_t stats;
} ld_parser_t;

uint8_t ld_crc8(const uint8_t *data, size_t len);

bool ld_decode_frame(const uint8_t raw[LD_FRAME_LEN], ld_frame_t *out);

void        ld_parser_init(ld_parser_t *p);
ld_status_t ld_parser_push_byte(ld_parser_t *p, uint8_t byte, ld_frame_t *out);

typedef void (*ld_frame_cb)(const ld_frame_t *frame, void *ctx);

size_t ld_parser_feed(ld_parser_t *p, const uint8_t *data, size_t len,
                      ld_frame_cb cb, void *ctx);

uint16_t ld_point_angle_cdeg(const ld_frame_t *f, size_t i);

#define LD_SCAN_MAX_POINTS 1024u

typedef struct {
    uint16_t angle_cdeg;
    uint16_t distance_mm;
    uint8_t  intensity;
} ld_scan_point_t;

typedef struct {
    ld_scan_point_t pts[LD_SCAN_MAX_POINTS];
    size_t   count;
    uint32_t dropped;         
    uint16_t speed_dps;       
    uint16_t first_timestamp_ms;
    bool     complete;        
} ld_scan_t;

typedef struct {
    ld_scan_t cur;
    uint16_t  last_start_cdeg;
    bool      have_last;
    bool      emitted_any;
} ld_scan_assembler_t;

void ld_scan_init(ld_scan_assembler_t *a);

bool ld_scan_add_frame(ld_scan_assembler_t *a, const ld_frame_t *f,
                       ld_scan_t *completed);

#ifndef __KERNEL__
int ld_serial_configure(int fd);
int ld_serial_open(const char *path);
#endif 

#ifdef __cplusplus
}
#endif

#endif
