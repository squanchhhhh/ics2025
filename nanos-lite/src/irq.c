#include "proc.h"
#include "syscall.h"
#include <common.h>
void do_syscall(Context *ctx);
static Context *do_event(Event e, Context *c) {
  switch (e.event) {
  case EVENT_YIELD:
    return schedule(c);
    // printf("Yield event recognized!\n");
    break;
  case EVENT_SYSCALL: {
    uintptr_t old_cp = (uintptr_t)c;
    do_syscall(c);
    if ((uintptr_t)current->cp != old_cp) {
      printf("[DEBUG] Context changed during syscall! Old: %p, New: %p\n",
             old_cp, current->cp);
    }
    return current->cp;
  }
  default:
    panic("Unhandled event ID = %d", e.event);
  }
  return c;
}

void init_irq(void) {
  Log("Initializing interrupt/exception handler...");
  cte_init(do_event);
}
