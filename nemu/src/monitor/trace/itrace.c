#include "trace/itrace.h"
#include "trace/tui.h"
#include <string.h>
#include <isa.h>
#define MAX_ITRACE 16
static ITraceEntry ie[MAX_ITRACE];
static uint64_t nr_i = 0; 

void push_inst(vaddr_t pc, const char *s, RegEntry *reg) {
    ITraceEntry *e = &ie[nr_i % MAX_ITRACE];
    e->pc = pc;
    strncpy(e->asmb, s, sizeof(e->asmb) - 1);
    e->asmb[sizeof(e->asmb) - 1] = '\0';
    
    if (reg != NULL) {
        e->reg = *reg; 
    } else {
        e->reg.reg_num = -1;
    }
    
    nr_i++;
}

void dump_insts() {
    printf("------- [ Recent Instructions (itrace) ] -------\n");
    uint64_t start = (nr_i > MAX_ITRACE) ? (nr_i - MAX_ITRACE) : 0;

    for (uint64_t i = start; i < nr_i; i++) {
        ITraceEntry *e = &ie[i % MAX_ITRACE];
        
        // 1. 整理反汇编文本
        char clean_asmb[64];
        strncpy(clean_asmb, e->asmb, 63);
        clean_asmb[63] = '\0';
        for (char *p = clean_asmb; *p; p++) if (*p == '\t') *p = ' ';

        // 2. 使用新接口 isa_reg_name 获取寄存器名
        char reg_info[64] = "";
        // 逻辑：只有当 reg_num > 0 (排除 $0) 且有效时才打印
        if (e->reg.reg_num > 0 && e->reg.reg_num < 32) {
            snprintf(reg_info, 63, " [%s:0x%x->0x%x]", 
                     isa_reg_name(e->reg.reg_num), 
                     e->reg.old_val, 
                     e->reg.new_val);
        }
        char filename[128]; int line = -1;
        get_pc_source(e->pc, filename, &line);
        char *code = (line != -1) ? get_src(filename, line) : "";
        char *short_name = strrchr(filename, '/') ? strrchr(filename, '/') + 1 : filename;
        if (line == -1) short_name = "none";
        printf("%s %-22s %-28s | %s:%-3d | %s", 
               (i == nr_i - 1 ? "-->" : "   "), 
               clean_asmb, 
               reg_info,
               short_name, line, 
               code);
    }
    printf("------------------------------------------------\n");
}