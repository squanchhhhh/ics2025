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
  bnez sp, from_user          # 如果 sp != 0，说明来自用户态

from_kernel:
  csrr sp, mscratch           # 来自内核态，拿回原始内核 sp
  j save_context

from_user:
  # 此时 sp 已经是内核栈，mscratch 存的是用户栈指针

save_context:
  addi sp, sp, -CONTEXT_SIZE   # 在内核栈开辟 Context 空间
  MAP(REGS, PUSH)              # 保存通用寄存器

  # 特殊处理 sp (x2)
  csrr t0, mscratch            
  sw t0, 8(sp)                 # 保存原始 sp 到 Context->gpr[2]

  # 状态标记：将 mscratch 置 0，表示当前已在内核中
  csrw mscratch, zero

  # 保存 CSR 寄存器
  csrr t0, mcause
  csrr t1, mstatus
  csrr t2, mepc
  sw t0, OFFSET_CAUSE(sp)
  sw t1, OFFSET_STATUS(sp)
  sw t2, OFFSET_EPC(sp)

  # --- 修改点 1：Linux 风格，不从硬件读回 satp ---
  # 因为 Context->pdir 在进程创建/切换时已经由内核定好了。
  # 我们不需要在每次中断时去覆盖它。

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

  # 检查是否返回内核态
  li t0, 0x1800                
  and t0, t1, t0
  bnez t0, restore_to_kernel   

restore_to_user:
  # 重新设置 mscratch 为内核栈顶
  addi t0, sp, CONTEXT_SIZE
  csrw mscratch, t0

  # --- 修改点 2：直接恢复页表，不再进行计算 ---
  # 此时 OFFSET_PDIR(sp) 里的值已经是 (Mode | PPN) 格式了
  lw t0, OFFSET_PDIR(sp)
  beqz t0, 1f                  
  csrw satp, t0
  sfence.vma                   
1:
  MAP(REGS, POP)               
  lw sp, 8(sp)                 # 恢复用户栈指针
  mret

restore_to_kernel:
  # 回到内核态，mscratch 保持为 0
  csrw mscratch, zero

  # --- 修改点 3：恢复页表逻辑极简化 ---
  lw t0, OFFSET_PDIR(sp)
  beqz t0, 1f
  csrw satp, t0
  sfence.vma
1:
  MAP(REGS, POP)               
  addi sp, sp, CONTEXT_SIZE    
  mret