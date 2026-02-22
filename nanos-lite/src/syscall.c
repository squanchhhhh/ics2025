#include "syscall.h"
#include "fs.h"
#include "proc.h"
#include <common.h>
#include <stdio.h>
struct timezone;
#define FB_ADDR 0xa1000000

#define KLOG(format, ...)                                                      \
  do {                                                                         \
    char _klog_buf[256];                                                       \
    int _klog_len =                                                            \
        snprintf(_klog_buf, sizeof(_klog_buf), format, ##__VA_ARGS__);         \
    for (int _i = 0; _i < _klog_len; _i++)                                     \
      putch(_klog_buf[_i]);                                                    \
  } while (0)

int sys_gettimeofday(struct timeval *tv, struct timezone *tz) {
  if (tv != NULL) {
    uint64_t us = io_read(AM_TIMER_UPTIME).us;
    tv->tv_sec = us / 1000000;
    tv->tv_usec = (uint32_t)(us % 1000000);
  }
  return 0;
}

static const char *syscall_names[] = {
  "SYS_exit", "SYS_yield", "SYS_open", "SYS_read", "SYS_write",
  "SYS_kill", "SYS_getpid", "SYS_close", "SYS_lseek", "SYS_brk",
  "SYS_fstat", "SYS_time", "SYS_signal", "SYS_execve", "SYS_fork",
  "SYS_link", "SYS_unlink", "SYS_wait", "SYS_times", 
  "SYS_gettimeofday", "SYS_mmap"
};

const char* get_syscall_name(int id) {
  if (id >= 0 && id < sizeof(syscall_names) / sizeof(syscall_names[0])) {
    return syscall_names[id];
  }
  return "SYS_unknown";
}
// GPR1 映射到 a7 (系统调用号)
// GPR2 映射到 a0 (第一个参数)
// GPR3 参数2
// GPR4 参数3
// GPRx 也映射到 a0 (返回值)

void do_syscall(Context *ctx) {
  uintptr_t a[7];
  a[0] = ctx->GPR1; // a7: syscall ID
  a[1] = ctx->GPR2; // a0: arg1
  a[2] = ctx->GPR3; // a1: arg2
  a[3] = ctx->GPR4; // a2: arg3
  a[4] = ctx->GPR5; // a3: arg4
  a[5] = ctx->GPR6; // a4: arg5
  a[6] = ctx->GPR7; // a5: arg6

  // 这里的 name=%s 是根据数组查的，可能不准，所以 case 内部的 log 才是真相
  KLOG("call syscall [ID=%x] name=%s\n", a[0], get_syscall_name(a[0]));

  switch (a[0]) {
  case SYS_yield:
    KLOG("  -> [Dispatch] SYS_yield\n");
    yield();
    ctx->GPRx = 0;
    break;

  case SYS_exit:
    KLOG("  -> [Dispatch] SYS_exit, code=%d\n", a[1]);
    Log("Process exited with code %d", a[1]);
    halt(0);
    break;

  case SYS_write:
    KLOG("  -> [Dispatch] SYS_write, fd=%d, buf=%p, len=%d\n", a[1], (void*)a[2], a[3]);
    ctx->GPRx = fs_write(a[1], (void *)a[2], a[3]);
    break;

  case SYS_open:
    KLOG("  -> [Dispatch] SYS_open, path=%s\n", (char *)a[1]);
    ctx->GPRx = fs_open((char *)a[1], a[2], a[3]);
    break;

  case SYS_brk:
    KLOG("  -> [Dispatch] SYS_brk, new_brk=%p\n", (void*)a[1]);
    uintptr_t new_brk = a[1];
    if (new_brk > current->max_brk) {
      uintptr_t va_start = ROUNDUP(current->max_brk, PGSIZE);
      uintptr_t va_end   = ROUNDUP(new_brk, PGSIZE);
      for (uintptr_t va = va_start; va < va_end; va += PGSIZE) {
        void *pa = new_page(1);
        map(&current->as, (void *)va, pa, 7); 
      }
      current->max_brk = new_brk; 
    }
    ctx->GPRx = 0;
    break;

  case SYS_gettimeofday:
    KLOG("  -> [Dispatch] SYS_gettimeofday, tv=%p\n", (void*)a[1]);
    ctx->GPRx = sys_gettimeofday((struct timeval *)a[1], (struct timezone *)a[2]);
    break;

  case SYS_close:
    KLOG("  -> [Dispatch] SYS_close, fd=%d\n", a[1]);
    ctx->GPRx = fs_close(a[1]);
    break;

  case SYS_read:
    KLOG("  -> [Dispatch] SYS_read, fd=%d, buf=%p, len=%d\n", a[1], (void*)a[2], a[3]);
    ctx->GPRx = fs_read(a[1], (void *)a[2], a[3]);
    break;

  case SYS_lseek:
    KLOG("  -> [Dispatch] SYS_lseek, fd=%d, off=%d, whence=%d\n", a[1], a[2], a[3]);
    ctx->GPRx = fs_lseek(a[1], a[2], a[3]);
    break;

  case SYS_mmap:
    KLOG("  -> [Dispatch] SYS_mmap, addr=%p, len=%x, fd=%d\n", (void*)a[1], a[2], a[5]);
    uintptr_t vaddr = a[1];
    size_t len = a[2];
    int fd = a[5];
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
      ctx->GPRx = -1;
      break;
    }
    if (file_table[fd].inum == 0xFFFFFFFF && strcmp(file_table[fd].name, "/dev/fb") == 0) {
      for (uintptr_t offset = 0; offset < len; offset += 4096) {
        map(&current->as, (void *)(vaddr + offset), (void *)(FB_ADDR + offset), 7);
      }
      ctx->GPRx = vaddr;
    } else {
      ctx->GPRx = -1;
    }
    break;

  case SYS_execve:
    KLOG("  -> [Dispatch] SYS_execve, path=%s\n", (char *)a[1]);
    do_execve((const char *)a[1], (char *const *)a[2], (char *const *)a[3]);
    break;

  default:
    KLOG("  -> [Dispatch] UNKNOWN SYSCALL ID=%d\n", a[0]);
    panic("Unhandled syscall ID = %d", a[0]);
  }
}