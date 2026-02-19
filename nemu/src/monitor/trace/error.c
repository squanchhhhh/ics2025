#include "trace/error.h"
#include <isa.h>
#define MAX_ERROR_LOG 16
ErrorEntry ee[MAX_ERROR_LOG];

void fill_unified_log() {
    // 清空旧的错误日志数组
    memset(ee, 0, sizeof(ee));

    for (int i = 0; i < MAX_ERROR_LOG; i++) {
        // i=0 是最新的记录，我们希望表格最后一行是最新的，所以可以倒序填充或打印
        ErrorEntry *e = &ee[i];

        // 1. 获取指令快照 (i 表示向后追溯多少条)
        get_error_itrace(e, i);

        // 如果这条记录根本不存在（程序刚运行没几行），就跳过
        if (e->ie.pc == 0) continue;

        // 2. 根据该指令的 PC，钩取函数名
        get_error_ftrace(e);

        // 3. 根据该指令的 PC，钩取对应的内存读写记录
        get_error_mtrace(e);
    }
}

void dump_unified_error() {
    fill_unified_log(); // 聚合数据

    printf("\n\033[1;33m======= [ Unified Execution Context (Last %d Steps) ] =======\033[0m\n", MAX_ERROR_LOG);
    
    // 表头定义
    printf("%-11s | %-15s | %-20s | %s\n", 
           "PC", "Instruction", "Function Scope", "Side Effects (Memory/Reg)");
    printf("------------|-----------------|----------------------|------------------------------------\n");

    // 倒序打印：从 ee[MAX_ERROR_LOG-1] 到 ee[0]
    // 这样最后一行就是程序崩溃/停止的那一条指令
    for (int i = MAX_ERROR_LOG - 1; i >= 0; i--) {
        ErrorEntry *e = &ee[i];
        if (e->ie.pc == 0) continue; // 跳过空记录

        // 1. 打印 PC 和 汇编
        // 如果是最后一条指令（i=0），用红色箭头标出
        if (i == 0) printf("\033[1;31m->\033[0m 0x%08x | ", e->ie.pc);
        else printf("   0x%08x | ", e->ie.pc);

        printf("%-15s | ", e->ie.asmb);

        // 2. 打印函数作用域 (由 get_error_ftrace 填充的 e->fe)
        printf("%-20s | ", e->fe);

        // 3. 打印副作用 (寄存器变化 和 内存访问)
        bool has_effect = false;

        // 寄存器副作用
        if (e->ie.reg.reg_num > 0 && e->ie.reg.old_val != e->ie.reg.new_val) {
            printf("\033[1;32m%s\033[0m:0x%x->0x%x ", 
                   isa_reg_name(e->ie.reg.reg_num), e->ie.reg.old_val, e->ie.reg.new_val);
            has_effect = true;
        }

        // 内存副作用 (由 get_error_mtrace 填充)
        if (e->me.pc != 0) {
            if (has_effect) printf("| ");
            printf("\033[1;36m[MEM]\033[0m %s 0x%08x=0x%lx ", 
                   e->me.type == MEM_READ ? "R" : "W", e->me.addr, e->me.data);
            has_effect = true;
        }

        if (!has_effect) printf("-");

        printf("\n");
    }

    printf("====================================================================================\n\n");
}