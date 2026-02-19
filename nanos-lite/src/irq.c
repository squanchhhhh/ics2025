#include <common.h>
#include "syscall.h"
#include "proc.h"
void do_syscall(Context *ctx);
static Context* do_event(Event e, Context* c) {
  if (c->pdir != NULL && (uintptr_t)c->pdir != 0x80aec000 && (uintptr_t)c->pdir != 0) {
     Log("CRITICAL: Context at %p is corrupted! pdir = %p", c, c->pdir);
     uint32_t *ptr = (uint32_t *)c;
     for(int i=0; i<5; i++) printf("offset %d: 0x%08x\n", i*4, ptr[i]);
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
