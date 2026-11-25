/*
 * lidar_ioctl.h
 *
 * Created on November 24, 2026
 * Author: Aaron Pineda
 *
 *
 * Based on aesd_ioctl.h (Dan Walkes, AESD assignment 9).
 */

#ifndef LIDAR_IOCTL_H
#define LIDAR_IOCTL_H

#include <linux/types.h>
#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

// Same limit as LD_SCAN_MAX_POINTS in ld_lidar.h 
#define LIDAR_MAX_POINTS 1024

struct lidar_point {
    __u16 angle_cdeg;    // 0.01 degree, clockwise, as the lidar reports
    __u16 distance_mm;   // 0 = no return
    __u8  intensity;
    __u8  reserved;      //pads the struct to 6 bytes
};

struct lidar_scan {
    __u64 timestamp_ns;  // note when the first frame of the scan arrived
    __u32 count;         // valid entries in pts[]
    __u32 dropped;       // points that did not fit in pts[]
    __u16 speed_dps;     // rotation speed, degrees per second
    __u16 reserved0;
    __u32 reserved1;     // pads the header to 24 bytes
    struct lidar_point pts[LIDAR_MAX_POINTS];
};

struct lidar_stats {
    __u32 frames_ok;        // valid CRC frames
    __u32 crc_errors;
    __u32 bytes_discarded;  // bytes skipped while looking for a frame header
    __u32 scans;            // fully assembled rotations
    __u32 scans_dropped;    // overwritten in the queue before anyone read them
    __u32 reserved;
};

#define LIDAR_IOC_MAGIC 0xCA

#define LIDAR_IOC_GET_STATS _IOR(LIDAR_IOC_MAGIC, 1, struct lidar_stats)
#define LIDAR_IOC_FLUSH     _IO(LIDAR_IOC_MAGIC, 2)
#define LIDAR_IOC_MAXNR     2

#endif /* LIDAR_IOCTL_H */
