#ifndef __COMMON_H__
#define __COMMON_H__

/* Uncomment these macros to enable corresponding functionality. */
#define HAS_CTE
//#define HAS_VME
//#define MULTIPROGRAM
//#define TIME_SHARING

#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <debug.h>
size_t ramdisk_read(void *buf, size_t offset, size_t len);
size_t ramdisk_write(const void *buf, size_t offset, size_t len);

//计算运行时间
#define TIME_START() uint64_t _start = io_read(AM_TIMER_UPTIME).us
#define TIME_END(name) do { \
    uint64_t _end = io_read(AM_TIMER_UPTIME).us; \
    if (_end - _start > 5000) { \
        printf("[Timer] %s took %llu us\n", name, _end - _start); \
    } \
} while(0)


#endif
