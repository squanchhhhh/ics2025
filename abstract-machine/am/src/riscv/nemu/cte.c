#include <am.h>
#include <riscv/riscv.h>
#include <klib.h>

static Context* (*user_handler)(Event, Context*) = NULL;

Context* __am_irq_handle(Context *c) {
  if (user_handler) {
    Event ev = {0};
    // 记录：中断发生的瞬间，当前 Context 的状态
     printf("\n[IRQ Entry] Context: %p, EPC: %x, Cause: %d, pdir: %p\n", 
             c, c->mepc, c->mcause, c->pdir);

    switch (c->mcause) {
      case 11: 
        if (c->GPR1 == -1) {
          ev.event = EVENT_YIELD;
        } else {
          ev.event = EVENT_SYSCALL;
        }
        c->mepc += 4;
        break;
      default: ev.event = EVENT_ERROR; break;
    }

    // 调用 user_handler (即 do_event)，并在内部调用 schedule
    Context *prev = c;
    c = user_handler(ev, c);
    
    // 记录：调度后的变化
    if (c != prev) {
       printf("[IRQ Switch] From Context %p (pdir %p) -> To %p (pdir %p)\n", 
               prev, prev->pdir, c, c->pdir);
    }
    
    assert(c != NULL);
  }

  // 这里的 __am_switch 负责真正的 SATP 硬件切换
  extern void __am_switch(Context *c);
  __am_switch(c);

  return c;
}

extern void __am_asm_trap(void);

bool cte_init(Context*(*handler)(Event, Context*)) {
  // initialize exception entry
  asm volatile("csrw mtvec, %0" : : "r"(__am_asm_trap));

  // register event handler
  user_handler = handler;

  return true;
}

Context* kcontext(Area kstack, void (*entry)(void *), void *arg) {
  Context *c = (Context *)((uintptr_t)kstack.end - sizeof(Context));
  memset(c, 0, sizeof(Context));
  c->mepc = (uintptr_t)entry;
  c->mstatus = 0x1800; 
  c->GPRx = (uintptr_t)arg; 
  printf("call kcontext set sp %p\n",kstack.end);
  c->gpr[2] = (uintptr_t)kstack.end; 
  c->pdir = NULL;
  return c;
}
void yield() {
#ifdef __riscv_e
  asm volatile("li a5, -1; ecall");
#else
  asm volatile("li a7, -1; ecall"); //系统调用号约定放在 a7
#endif
}

bool ienabled() {
  return false;
}

void iset(bool enable) {
}
