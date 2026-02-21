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
#define GPR1 gpr[17] // a7 系统调用号
#endif

#define GPR2 gpr[10] // a0: 参数 1 / 返回值
#define GPR3 gpr[11] // a1: 参数 2
#define GPR4 gpr[12] // a2: 参数 3
#define GPR5 gpr[13] // a3: 参数 4
#define GPR6 gpr[14] // a4: 参数 5
#define GPR7 gpr[15] // a5: 参数 6
#define GPRx gpr[10] // a0: 返回值
#endif
