#include "trace/error.h"
#include <isa.h>
#include "trace/tui.h"
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
    
    // 重新定义表头，移除独立 PC 列
    // 宽度分配：Instruction(35), Function(25), Side Effects(剩余)
    printf("%-35s | %-25s | %s\n", 
           "Instruction (with PC)", "Function Scope", "Side Effects (Memory/Reg)");
    printf("------------------------------------|---------------------------|------------------------------------\n");

    for (int i = MAX_ERROR_LOG - 1; i >= 0; i--) {
        ErrorEntry *e = &ee[i];
        if (e->ie.pc == 0) continue; 

        // 1. 打印带箭头的指令列
        if (i == 0) printf("\033[1;31m->\033[0m ");
        else printf("   ");

        // 直接打印 ie.asmb，它通常已经包含了 "0x8000...: inst" 格式
        printf("%-32s | ", e->ie.asmb);

        // 2. 打印函数作用域
        printf("%-25s | ", e->fe);

        // 3. 打印副作用
        bool has_effect = false;
        if (e->ie.reg.reg_num > 0 && e->ie.reg.old_val != e->ie.reg.new_val) {
            printf("\033[1;32m%s\033[0m:0x%x->0x%x ", 
                   isa_reg_name(e->ie.reg.reg_num), e->ie.reg.old_val, e->ie.reg.new_val);
            has_effect = true;
        }

        if (e->me.pc != 0) {
            if (has_effect) printf("| ");
            printf("\033[1;36m[MEM]\033[0m %s 0x%08x=0x%lx ", 
                   e->me.type == MEM_READ ? "R" : "W", e->me.addr, e->me.data);
            has_effect = true;
        }

        if (!has_effect) printf("-");
        printf("\n");

        // --- 附加功能：打印对应的源代码行 ---
        // 只有在 line 有效时打印，使用灰色显示以示区分
        char filename[128]; int line;
        get_pc_source(e->ie.pc, filename, &line);
        if (line != -1) {
            char *src = get_src(filename, line);
            if (src && src[0] != '\0') {
                // 去掉源码开头的空白符，稍微缩进一下
                while (*src == ' ' || *src == '\t') src++;
                printf("      \033[1;90m[src] %s:%d: %s\033[0m\n", 
                       (strrchr(filename, '/') ? strrchr(filename, '/') + 1 : filename), 
                       line, src);
            }
        }
    }

    printf("====================================================================================\n\n");
}