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
   * 1. 灵魂交换：判断是从用户态还是内核态进来的
   * 约定：mscratch 处平时存放着该进程的“内核栈顶”；如果在内核中运行，mscratch 为 0。
   */
  csrrw sp, mscratch, sp      # 原子交换 sp 和 mscratch
  bnez sp, save_context       # 如果交换后 sp != 0，说明来自用户态，此时 sp 已变成内核栈，去保存现场
  
  # 走到这里说明 sp 为 0，即来自内核态（嵌套中断）
  csrrw sp, mscratch, sp      # 再次交换回来，让 sp 恢复为当前的内核栈指针，继续向下运行

save_context:
  /*
   * 2. 保存现场：在内核栈上开辟空间并压入所有寄存器
   */
  addi sp, sp, -CONTEXT_SIZE   # 在内核栈上预留 Context 结构体的空间
  MAP(REGS, PUSH)              # 宏展开：依次执行 sw x1, x3-x31 到栈上（跳过 x2）

  # 特殊处理 x2 (sp)：
  # 如果来自用户态，中断前的真实 sp 现在寄存在 mscratch 里
  # 如果来自内核态，刚才第二次交换后，mscratch 存的是 0（这个逻辑后续需要精修，目前假设主要处理用户态切换）
  csrr t0, mscratch            
  STORE t0, OFFSET_SP(sp)      # 将中断前的原始栈指针存入 Context->gpr[2]

  # 保存 CSR 控制寄存器（异常原因、状态、返回地址）
  csrr t0, mcause
  csrr t1, mstatus
  csrr t2, mepc
  STORE t0, OFFSET_CAUSE(sp)
  STORE t1, OFFSET_STATUS(sp)
  STORE t2, OFFSET_EPC(sp)

  csrw mscratch, zero
  /*
   * 3. 核心处理：调用 C 语言的中断处理程序
   */
  mv a0, sp                    # 参数 a0 = Context 结构体指针
  call __am_irq_handle         # 调用 nanos-lite 的处理逻辑
  
  # 关键点：a0 是 __am_irq_handle 的返回值，即调度器选出的“下一个进程”的 Context 指针
  mv sp, a0                    # ！！！正式切换栈指针，sp 此时可能已经跳到了另一个进程的内核栈

  /*
   * 4. 准备恢复现场
   */
  # 恢复目标进程的 sp 到 mscratch 暂存
  # 这样在 mret 之前的最后时刻，我们可以通过交换把这个 sp 换给硬件
  LOAD t0, OFFSET_SP(sp)
  csrw mscratch, t0            # 现在 mscratch = 目标进程的原始栈指针（如 0x7ffffff8）

  # 恢复 CSR 状态，准备返回
  LOAD t1, OFFSET_STATUS(sp)
  LOAD t2, OFFSET_EPC(sp)
  csrw mstatus, t1
  csrw mepc, t2

  # 恢复除 sp 以外的所有通用寄存器
  MAP(REGS, POP)

  /*
   * 5. 最终退出：跨越特权级屏障
   */
  addi sp, sp, CONTEXT_SIZE    # 释放内核栈空间，此时 sp 回到内核栈顶（如 0x802ecf70）
  
  # 临门一脚：交换
  # 交换后：sp 变回了目标进程的原始栈指针（如 0x7ffffff8）
  #        mscratch 变回了目标进程的内核栈顶（如 0x802ecf70），为下次中断做好准备！
  csrrw sp, mscratch, sp       
  
  mret                         # 硬件动作：根据 mepc 跳回，根据 mstatus 改变特权级