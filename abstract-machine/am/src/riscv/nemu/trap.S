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
   */
  csrrw sp, mscratch, sp      # 交换 sp 和 mscratch
  bnez sp, from_user          # 如果 sp != 0，说明来自用户态（mscratch 原本存着内核栈）

from_kernel:
  csrr sp, mscratch           # 如果来自内核态，刚才那句交换让 sp 变 0 了，现在从 mscratch 拿回原始内核 sp
  j save_context

from_user:
  # 此时 sp 已经是内核栈，mscratch 存的是用户栈指针

save_context:
  addi sp, sp, -CONTEXT_SIZE   # 在内核栈开辟 Context 空间
  MAP(REGS, PUSH)              # 保存通用寄存器 x1, x3-x31 (宏定义)

  # 特殊处理 sp (x2)
  csrr t0, mscratch            # 取出异常发生前的原始 sp (用户栈或内核栈)
  sw t0, 8(sp)                 # 保存到 Context->gpr[2] (2 * 4 = 8)

  # 状态标记：将 mscratch 置 0
  # 表示当前已在内核中，若发生嵌套异常，from_kernel 逻辑能正确触发
  csrw mscratch, zero

  # 保存 CSR 寄存器
  csrr t0, mcause
  csrr t1, mstatus
  csrr t2, mepc
  sw t0, OFFSET_CAUSE(sp)
  sw t1, OFFSET_STATUS(sp)
  sw t2, OFFSET_EPC(sp)

  # 保存页表基址
  csrr t0, satp
  sw t0, OFFSET_PDIR(sp)

  /*
   * 2. 调用 C 语言事件分发器
   */
  mv a0, sp                    # a0 = Context *c
  call __am_irq_handle         
  
  # 关键：切换到 handle 返回的新 Context (实现进程切换)
  mv sp, a0                    

  /*
   * 3. 现场恢复
   */
  # 恢复 CSR
  lw t1, OFFSET_STATUS(sp)
  lw t2, OFFSET_EPC(sp)
  csrw mstatus, t1
  csrw mepc, t2

  # 检查 mstatus.MPP (bits 12:11)
  # MPP == 0 (User), MPP == 3 (Machine)
  li t0, 0x1800                
  and t0, t1, t0
  bnez t0, restore_to_kernel   # 如果返回内核态 (MPP != 0)，走专用路径

restore_to_user:
  # 只有回到用户态，才需要重新设置 mscratch 为内核栈顶 (Context 之后的位置)
  addi t0, sp, CONTEXT_SIZE
  csrw mscratch, t0

  # 恢复页表 (VME)
  lw t0, OFFSET_PDIR(sp)
  csrw satp, t0

  MAP(REGS, POP)               # 恢复通用寄存器
  lw sp, 8(sp)                 # 直接从 Context 恢复用户栈指针 (x2)
  
  # 此时 sp 是用户栈，mscratch 是内核栈顶
  # 注意：这里不需要再执行 csrrw，因为 sp 已经通过 lw 恢复了
  mret

restore_to_kernel:
  # 如果回到内核态，mscratch 必须保持为 0
  csrw mscratch, zero

  # 恢复页表
  lw t0, OFFSET_PDIR(sp)
  csrw satp, t0

  MAP(REGS, POP)               # 恢复通用寄存器
  addi sp, sp, CONTEXT_SIZE    # 简单平移栈指针释放空间
  
  # 此时 sp 回到了内核态 yield/trap 之前的状态
  # 且 mscratch 依然是 0
  mret