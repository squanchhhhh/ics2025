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
#define OFFSET_CAUSE  ((NR_REGS + 0) * XLEN)
#define OFFSET_STATUS ((NR_REGS + 1) * XLEN)
#define OFFSET_EPC    ((NR_REGS + 2) * XLEN)

.align 3
.globl __am_asm_trap
__am_asm_trap:
  /*
   * 1. 栈切换与身份识别
   */
  csrrw sp, mscratch, sp      # 交换 sp 和 mscratch
  bnez sp, from_user          # 如果交换后 sp != 0，说明来自用户态

from_kernel:
  csrr sp, mscratch           # 如果来自内核态，把被换出去的内核 sp 还原回来
  j save_context

from_user:
  # 此时 sp 已经是内核栈指针，mscratch 存的是用户栈指针
  # 无需额外操作，直接向下保存

save_context:
  addi sp, sp, -CONTEXT_SIZE
  MAP(REGS, PUSH)

  # 特殊处理 sp
  # 我们需要知道中断发生前，sp 到底在哪里
  csrr t0, mscratch
  STORE t0, OFFSET_SP(sp)

  # 如果是从用户态来的，现在把 mscratch 清零，标记内核状态
  # 这样如果在内核里再发生异常，bnez sp 就会失败，走 from_kernel 分支
  csrw mscratch, zero

  # 保存 CSR
  csrr t0, mcause
  csrr t1, mstatus
  csrr t2, mepc
  STORE t0, OFFSET_CAUSE(sp)
  STORE t1, OFFSET_STATUS(sp)
  STORE t2, OFFSET_EPC(sp)

  /*
   * 2. 调用 C 语言处理函数
   */
  mv a0, sp
  call __am_irq_handle
  
  # a0 现在是下一个进程的 Context 指针
  mv sp, a0

  /*
   * 3. 准备返回
   */
  # 恢复 CSR
  LOAD t1, OFFSET_STATUS(sp)
  LOAD t2, OFFSET_EPC(sp)
  csrw mstatus, t1
  csrw mepc, t2

  # 关键：判断我们要返回到哪里？
  # 检查 mstatus.MPP (bits 11-12)
  # 如果 MPP != 0 (Machine Mode)，说明返回内核，mscratch 应该保持为 0
  # 如果 MPP == 0 (User Mode)，说明返回用户态，mscratch 应该设为内核栈顶
  li t0, 0x1800
  and t0, t1, t0
  bnez t0, restore_regs       # 如果要回到内核，跳过 mscratch 设置

  # 只有回到用户态才设置 mscratch
  addi t0, sp, CONTEXT_SIZE
  csrw mscratch, t0

restore_regs:
  MAP(REGS, POP)

  # 释放内核栈空间
  addi sp, sp, CONTEXT_SIZE

  # 最后一次灵魂交换
  # 如果回到用户态：sp 变回用户栈，mscratch 变回内核栈顶
  # 如果回到内核态：sp 没变（因为 mscratch 之前是 0），mscratch 依然是 0
  csrrw sp, mscratch, sp
  
  mret