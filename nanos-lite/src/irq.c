#include <common.h>
#include "syscall.h"
#include "proc.h"
void do_syscall(Context *ctx);
static Context* do_event(Event e, Context* c) {
  if ((uintptr_t)c < 0x80000000) {
    printf("\n[BUG DETECTED] Context is in User Space at %p!\n", c);
    printf("pdir content: 0x%08x\n", c->pdir);
    unsigned char *raw = (unsigned char *)c;
    printf("Raw data near pdir: ");
    for(int i = 0; i < 16; i++) printf("%02x ", raw[140 + i]); // 140 是 pdir 偏移
    printf("\n");
    panic("Context safely violation.");
  }
  switch (e.event) {
    case EVENT_YIELD:
      return schedule(c);
      //printf("Yield event recognized!\n");
      break;
    case EVENT_SYSCALL:
      //printf("Syscall event recognized!\n");
      do_syscall(c);
      break;
    default: 
      panic("Unhandled event ID = %d", e.event);
  }
  return c;
}

void init_irq(void) {
  Log("Initializing interrupt/exception handler...");
  cte_init(do_event);
}
