#include "ld_lidar.h"

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

static void scan_reset(ld_scan_t *s)
{
    s->count = 0;
    s->dropped = 0;
    s->speed_dps = 0;
    s->first_timestamp_ms = 0;
    s->complete = false;
}

void ld_scan_init(ld_scan_assembler_t *a)
{
    memset(a, 0, sizeof(*a));
    scan_reset(&a->cur);
}

static void scan_append(ld_scan_t *s, const ld_frame_t *f)
{
    if (s->count == 0)
        s->first_timestamp_ms = f->timestamp_ms;
    s->speed_dps = f->speed_dps;

    for (size_t i = 0; i < LD_POINTS_PER_FRAME; i++) {
        if (s->count >= LD_SCAN_MAX_POINTS) {
            s->dropped++;
            continue;
        }
        ld_scan_point_t *dst = &s->pts[s->count++];
        dst->angle_cdeg  = ld_point_angle_cdeg(f, i);
        dst->distance_mm = f->points[i].distance_mm;
        dst->intensity   = f->points[i].intensity;
    }
}

bool ld_scan_add_frame(ld_scan_assembler_t *a, const ld_frame_t *f,
                       ld_scan_t *completed)
{
    bool emitted = false;
    uint16_t start = (uint16_t)(f->start_angle_cdeg % LD_ANGLE_FULL_CDEG);

    if (a->have_last && start < a->last_start_cdeg && a->cur.count > 0) {
        a->cur.complete = a->emitted_any;
        if (completed)
            *completed = a->cur;
        a->emitted_any = true;
        emitted = true;
        scan_reset(&a->cur);
    }

    scan_append(&a->cur, f);
    a->last_start_cdeg = start;
    a->have_last = true;
    return emitted;
}
