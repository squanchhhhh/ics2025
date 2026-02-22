#include <common.h>
#include "syscall.h"
#include "proc.h"
void do_syscall(Context *ctx);
static Context* do_event(Event e, Context* c) {
  switch (e.event) {
    case EVENT_YIELD:
      return schedule(c);
      //printf("Yield event recognized!\n");
      break;
    case EVENT_SYSCALL:
      //printf("Syscall event recognized!\n");
      do_syscall(c);
      printf("[DEBUG] Syscall done. Old c: %p, New cp: %p\n", c, current->cp);
      return current->cp;
    default: 
      panic("Unhandled event ID = %d", e.event);
  }
  return c;
}

void init_irq(void) {
  Log("Initializing interrupt/exception handler...");
  cte_init(do_event);
}
