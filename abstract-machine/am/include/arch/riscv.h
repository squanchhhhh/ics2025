#ifndef ARCH_H__
#define ARCH_H__

#ifdef __riscv_e
#define NR_REGS 16
#else
#define NR_REGS 32
#endif
struct Context {
  uintptr_t gpr[NR_REGS]; // 偏移 0 到 124 
  uintptr_t mcause;       // 偏移 128
  uintptr_t mstatus;      // 偏移 132
  uintptr_t mepc;         // 偏移 136
  void *pdir;            
};

#ifdef __riscv_e
#define GPR1 gpr[15] // a5
#else
#define GPR1 gpr[17] // a7
#endif

#define GPR2 gpr[10] // a0: 第 1 个参数
#define GPR3 gpr[11] // a1: 第 2 个参数
#define GPR4 gpr[12] // a2: 第 3 个参数
#define GPRx gpr[10] // a0: 返回值寄存器
#endif
