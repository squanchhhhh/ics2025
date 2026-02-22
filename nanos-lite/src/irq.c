#include <common.h>
#include "syscall.h"
#include "proc.h"
void do_syscall(Context *ctx);
static Context* do_event(Event e, Context* c) {
  printf("Event %d processed, returning context at %p\n", e.event, c);
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
