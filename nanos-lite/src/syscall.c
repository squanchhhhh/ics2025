#include <common.h>
#include <stdio.h>
#include "syscall.h"
#include "fs.h"
#include "proc.h"
struct timezone;

int sys_gettimeofday(struct timeval *tv, struct timezone *tz) {
  if (tv != NULL) {
    uint64_t us = io_read(AM_TIMER_UPTIME).us;
    tv->tv_sec = us / 1000000;   
    tv->tv_usec = (uint32_t)(us % 1000000); 
  }
  return 0; 
}
//GPR1 映射到 a7 (系统调用号)
//GPR2 映射到 a0 (第一个参数)
//GPR3 参数2
//GPR4 参数3
//GPRx 也映射到 a0 (返回值)

void do_syscall(Context *ctx) {
  uintptr_t a[4];
  a[0] = ctx->GPR1; 
  a[1] = ctx->GPR2; 
  a[2] = ctx->GPR3; 
  a[3] = ctx->GPR4;
  printf("call syscall a[0]=%d, a[1]=%d, a[2]=%x, a[3]=%d\n",a[0],a[1],a[2],a[3]);
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

    case SYS_brk: 
      ctx->GPRx = 0; 
      break;

    case SYS_gettimeofday:
      ctx->GPRx = sys_gettimeofday((struct timeval *)ctx->GPR2, (struct timezone *)ctx->GPR3);
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
      //todo实现真正的mmap
        ctx->GPRx = 0xa1000000;
       // printf("Kernel mmap: return 0x%x\n", ctx->GPRx);
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
        Log("  Execve failed: file '%s' not found. Returning -2 (ENOENT).", fname);
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