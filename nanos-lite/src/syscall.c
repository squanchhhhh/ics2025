#include <common.h>
#include "syscall.h"
size_t sys_write(int fd, const void *buf, size_t count) {
  if (fd == 1 || fd == 2) {
    for (size_t i = 0; i < count; i++) {
      putch(((char *)buf)[i]); 
    }
    return count;
  }
  return -1;
}

void do_syscall(Context *ctx) {
  uintptr_t a[4];
  a[0] = ctx->GPR1; 
  a[1] = ctx->GPR2; 
  a[2] = ctx->GPR3; 
  a[3] = ctx->GPR4;

  switch (a[0]) {
    case SYS_yield:
      yield();
      ctx->GPRx = 0; 
      break;

    case SYS_exit:
      Log("Process exited with code %d", a[1]);
      halt(0); 
      break;

    case SYS_write: 
      ctx->GPRx = sys_write(a[1], (void *)a[2], a[3]);
      break;
    default: 
      panic("Unhandled syscall ID = %d", a[0]);
  }
}