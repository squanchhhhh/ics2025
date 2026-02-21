#define concat_temp(x, y) x ## y
#define concat(x, y) concat_temp(x, y)
#define MAP(c, f) c(f)

#if __riscv_xlen == 32
#define LOAD  lw
#define STORE sw
#define XLEN  4
#else
#define LOAD  ld
#define STORE sd
#define XLEN  8
#endif

#define REGS_LO16(f) \
      f( 1)       f( 3) f( 4) f( 5) f( 6) f( 7) f( 8) f( 9) \
f(10) f(11) f(12) f(13) f(14) f(15)
#ifndef __riscv_e
#define REGS_HI16(f) \
                                    f(16) f(17) f(18) f(19) \
f(20) f(21) f(22) f(23) f(24) f(25) f(26) f(27) f(28) f(29) \
f(30) f(31)
#define NR_REGS 32
#else
#define REGS_HI16(f)
#define NR_REGS 16
#endif

#define REGS(f) REGS_LO16(f) REGS_HI16(f)

#define PUSH(n) STORE concat(x, n), (n * XLEN)(sp);
#define POP(n)  LOAD  concat(x, n), (n * XLEN)(sp);

#define CONTEXT_SIZE  ((NR_REGS + 4) * XLEN)
#define OFFSET_SP     ( 2 * XLEN)
#define OFFSET_CAUSE  (32 * XLEN)  // 128
#define OFFSET_STATUS (33 * XLEN)  // 132
#define OFFSET_EPC    (34 * XLEN)  // 136
#define OFFSET_PDIR   (35 * XLEN)  // 140

.align 3
.globl __am_asm_trap
__am_asm_trap:
  /*
   * 1. 栈切换
   */
  csrrw sp, mscratch, sp      # 交换 sp 和 mscratch
  bnez sp, from_user          # 如果 sp 不为 0，说明之前在用户态

from_kernel:
  csrr sp, mscratch           # 如果之前就在内核态，把 sp 换回来
  j save_context

from_user:
  # 此时 sp 已是内核栈，mscratch 是用户栈

save_context:
  addi sp, sp, -CONTEXT_SIZE
  MAP(REGS, PUSH)

  # 特殊处理 sp (x2)，将其保存到 Context->gpr[2]
  csrr t0, mscratch
  STORE t0, OFFSET_SP(sp)

  # 状态标记：进入内核后 mscratch 置 0
  csrw mscratch, zero

  # 保存 CSR
  csrr t0, mcause
  csrr t1, mstatus
  csrr t2, mepc
  STORE t0, OFFSET_CAUSE(sp)
  STORE t1, OFFSET_STATUS(sp)
  STORE t2, OFFSET_EPC(sp)

  # 注意：完全不碰 OFFSET_PDIR(sp)！
  # 里面的值由 C 语言在进程创建或 __am_switch 时填入

  /*
   * 2. 调用 C 语言处理函数
   */
  mv a0, sp                   # a0 = Context *
  call __am_irq_handle
  mv sp, a0                   # 切换到返回的 Context (可能是新的进程)

  /*
   * 3. 恢复现场
   */
  LOAD t1, OFFSET_STATUS(sp)
  LOAD t2, OFFSET_EPC(sp)
  csrw mstatus, t1
  csrw mepc, t2

  # 检查返回模式
  li t0, 0x1800               # MPP 位
  and t0, t1, t0
  bnez t0, restore_to_kernel

restore_to_user:
  # 准备下一次进入内核的栈顶
  addi t0, sp, CONTEXT_SIZE
  csrw mscratch, t0
  
  # --- 只有这一处保留 SATP 恢复，用于进程间切换 ---
  LOAD t0, OFFSET_PDIR(sp)
  # 这里不加检查，因为 C 语言必须保证 PDIR 是有效的
  csrw satp, t0
  # sfence.vma  # 可选：如果 __am_switch 里刷过了，这里可以不刷

  MAP(REGS, POP)
  LOAD sp, OFFSET_SP(sp)      # 恢复用户栈
  mret

restore_to_kernel:
  csrw mscratch, zero
  
  # 内核态返回不切页表，因为 Linux 模式下内核映射是全页表共享的
  MAP(REGS, POP)
  addi sp, sp, CONTEXT_SIZE
  mret