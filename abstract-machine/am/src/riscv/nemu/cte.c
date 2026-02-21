#include <am.h>
#include <riscv/riscv.h>
#include <klib.h>

static Context* (*user_handler)(Event, Context*) = NULL;

Context* __am_irq_handle(Context *c) {
  if (user_handler) {
    Event ev = {0};
    
    // Log: 进入异常时的瞬时状态
    // 如果 EPC 总是同一个值，说明程序卡死在该指令
    // printf("\n[CTE] Trap! EPC=0x%x, Cause=%d, a7=%d\n", c->mepc, c->mcause, c->GPR1);

    switch (c->mcause) {
      case 11: // Machine environment call (ecall)
        // 根据约束，GPR1 (a7) 为 -1 表示 yield
        if (c->GPR1 == -1) {
          ev.event = EVENT_YIELD;
        } else {
          ev.event = EVENT_SYSCALL;
        }
        // 关键：ecall 触发的异常 mepc 指向 ecall 本身
        // 必须 +4 跳过，否则 mret 后会无限循环执行 ecall
        c->mepc += 4;
        break;
      default: 
        ev.event = EVENT_ERROR; 
        printf("\033[1;31m[CTE] Unhandled mcause: %d at EPC: 0x%x\033[0m\n", c->mcause, c->mepc);
        break;
    }
    // 调用 nanos-lite 的 do_event，进而调用 schedule()
    c = user_handler(ev, c);
    assert(c != NULL);
  }

  // 这里的 __am_switch 负责真正的 SATP (页表) 硬件切换
  // 如果此行导致崩溃，请检查页表是否进行了 Identity Mapping (恒等映射)
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
  // 1. 让 Context 住在栈的最顶端
  Context *c = (Context *)((uintptr_t)kstack.end - sizeof(Context));
  memset(c, 0, sizeof(Context));
  
  c->mepc = (uintptr_t)entry;
  c->GPRx = (uintptr_t)arg;
  c->pdir = NULL;

  // 2. 【关键修正】：初始 sp 绝对不能等于 c！
  // 我们要让程序开始运行时的 sp，处于 c 的下方（更低的地址）
  // 这样 printf 往高地址存参数时，才不会踩到 Context 结构体本身
  c->gpr[2] = (uintptr_t)c - 32; // 预留 32 字节或更多的安全空隙
  
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
