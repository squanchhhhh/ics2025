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
  KLOG("call syscall a[0]=%x, a[1]=%x, a[2]=%x, a[3]=%x,a[4]=%x,a[5]=%x,a[6]=%x\n", a[0], a[1], a[2],
       a[3],a[4],a[5],a[6]);
  switch (a[0]) {
  case SYS_yield:
    yield();
    ctx->GPRx = 0;
    break;

  case SYS_exit:
    Log("Process exited with code %d", a[1]);
    halt(0);
    /*
    printf("proc %s quit\n",current->name);
    for (int i = 3; i < MAX_NR_PROC_FILE; i++) {
        if (current->fd_table[i] != -1) {
            fs_close(i);
            current->fd_table[i] = -1;
        }
    }
    do_execve("/bin/nterm");
    */
    break;

  case SYS_write:
    ctx->GPRx = fs_write(a[1], (void *)a[2], a[3]);
    break;

  case SYS_open:
    ctx->GPRx = fs_open((char *)a[1], a[2], a[3]);
    break;

case SYS_brk: {
  printf("SYS_brk: current->max_brk = %p, new_brk = %p\n", 
          (void*)current->max_brk, (void*)a[1]);
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
}

  case SYS_gettimeofday:
    ctx->GPRx = sys_gettimeofday((struct timeval *)ctx->GPR2,
                                 (struct timezone *)ctx->GPR3);
    break;

  case SYS_close:
    ctx->GPRx = fs_close(a[1]);
    break;

  case SYS_read:
    ctx->GPRx = fs_read(a[1], (void *)a[2], a[3]);
    break;

  case SYS_lseek:
    ctx->GPRx = fs_lseek(a[1], a[2], a[3]);
    break;
  case SYS_mmap: {
    uintptr_t vaddr = a[1];
    size_t len = a[2];
    int fd = a[5]; // 确保你已经扩展了 GPR 映射到 a4

    // 1. 边界与合法性检查
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
      ctx->GPRx = -1;
      break;
    }

    // 2. 识别是否为显存设备
    // 我们可以根据你在 mount_devtmpfs 中设置的 inum 或名字来判断
    if (file_table[fd].inum == 0xFFFFFFFF &&
        strcmp(file_table[fd].name, "/dev/fb") == 0) {

      // 3. 核心：建立页表映射
      // 物理地址 FB_ADDR 在 nemu.h 中定义为 0xa1000000
      for (uintptr_t offset = 0; offset < len; offset += 4096) {
        // 调用 VME 接口：将用户虚拟页映射到显存物理页
        // 权限必须给 MMAP_READ | MMAP_WRITE (即 PTE_R | PTE_W | PTE_U)
        map(&current->as, (void *)(vaddr + offset), (void *)(FB_ADDR + offset),
            MMAP_READ | MMAP_WRITE);
      }

      KLOG("VFS: mmap /dev/fb to vaddr %p, size %d\n", (void*)vaddr, len);
      ctx->GPRx = vaddr; // 返回映射后的起始虚拟地址
    } else {
      // 普通磁盘文件暂不支持 mmap，或者返回错误
      ctx->GPRx = -1;
    }
    break;
  }
  case SYS_execve: {
    const char *fname = (const char *)a[1];
    char *const *argv = (char *const *)a[2];
    char *const *envp = (char *const *)a[3];

    // 1. 打印尝试执行的文件名
    Log("Syscall SYS_execve: fname = %s", fname);

    // 2. 打印传入的 argv，确认 Shell 到底想干嘛
    if (argv) {
      for (int i = 0; argv[i] != NULL; i++) {
        Log("  argv[%d] = %s", i, argv[i]);
      }
    } else {
      Log("  argv = NULL");
    }

    int fd = fs_open(fname, 0, 0);
    if (fd < 0) {
      // 3. 重点：如果找不到文件，打印提示
      Log("  Execve failed: file '%s' not found. Returning -2 (ENOENT).",
          fname);
      ctx->GPRx = -2;
    } else {
      fs_close(fd);
      Log("  Execve success: loading '%s'...", fname);
      do_execve(fname, argv, envp);
      // 注意：如果 do_execve 正常工作，它会替换上下文，代码通常不会执行到这里
    }
    break;
  }

  default:
    panic("Unhandled syscall ID = %d", a[0]);
  }
}