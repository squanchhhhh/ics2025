#include <common.h>
#include "syscall.h"
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

    default: 
      panic("Unhandled syscall ID = %d", a[0]);
  }
}