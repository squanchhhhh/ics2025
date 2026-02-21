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
   * 1. 栈切换逻辑
   * mscratch 初值为 0。如果是从用户态进入，mscratch 存有内核栈顶。
   */
  csrrw sp, mscratch, sp      # 交换 sp 和 mscratch
  bnez sp, from_user          # 如果交换后 sp != 0，说明之前 sp 存的是内核栈，来自用户态

from_kernel:
  csrr sp, mscratch           # 如果来自内核态，还原被换走的内核 sp
  j save_context

from_user:
  # 此时 sp 已是内核栈，mscratch 存的是用户栈指针

save_context:
  addi sp, sp, -CONTEXT_SIZE   # 在内核栈开辟 Context 空间
  MAP(REGS, PUSH)              # 保存通用寄存器 x1, x3-x31

  # 特殊处理 sp (x2)
  csrr t0, mscratch            # 取出异常发生前的原始 sp
  STORE t0, (2 * XLEN)(sp)     # 保存到 Context->gpr[2]

  # 状态标记：将 mscratch 置 0，表示当前已在内核中
  # 防止内核态嵌套异常时再次发生错误的栈切换
  csrw mscratch, zero

  # 保存 CSR 寄存器 (关键现场)
  csrr t0, mcause
  csrr t1, mstatus
  csrr t2, mepc
  STORE t0, OFFSET_CAUSE(sp)
  STORE t1, OFFSET_STATUS(sp)
  STORE t2, OFFSET_EPC(sp)

  # --- 核心修复：保存页表基址 (VME 相关) ---
  # 将当前 satp 存入 Context->pdir，防止返回时读取到随机脏数据
  csrr t0, satp
  STORE t0, OFFSET_PDIR(sp)

  /*
   * 2. 调用 C 语言事件分发器
   */
  mv a0, sp                    # a0 = Context *c
  call __am_irq_handle         # 处理事件
  
  # 注意：__am_irq_handle 可能会调用 schedule() 返回一个新的 Context 指针
  mv sp, a0                    # 切换到目标进程的栈

  /*
   * 3. 现场恢复
   */
  # 恢复 CSR
  LOAD t1, OFFSET_STATUS(sp)
  LOAD t2, OFFSET_EPC(sp)
  csrw mstatus, t1
  csrw mepc, t2

  # 决定返回后的栈状态
  # 检查 mstatus.MPP (Machine Previous Privilege)
  # 如果 MPP == 0 (User Mode)，说明要返回用户态
  li t0, 0x1800                # MPP 掩码
  and t0, t1, t0
  bnez t0, restore_regs        # 如果 MPP != 0 (内核态)，跳过 mscratch 重置

  # 只有回到用户态，才需要重新设置 mscratch 为内核栈顶
  # 这样下一次用户态 ecall 才能找到内核栈
  addi t0, sp, CONTEXT_SIZE
  csrw mscratch, t0

restore_regs:
  # --- 核心修复：页表切换 ---
  # 恢复目标进程的 satp (页表)
  LOAD t0, OFFSET_PDIR(sp)
  csrw satp, t0

  MAP(REGS, POP)               # 恢复通用寄存器
  addi sp, sp, CONTEXT_SIZE    # 释放 Context 空间

  # 最后一次 sp 交换
  # 返回用户态：sp 恢复为用户栈，mscratch 保存内核栈顶
  # 返回内核态：sp 保持内核栈，mscratch 保持为 0
  csrrw sp, mscratch, sp
  
  mret