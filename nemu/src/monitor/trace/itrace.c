#include "trace/itrace.h"
#include "trace/tui.h"
#include <string.h>

#define MAX_I_RING_SIZE 16
static ITraceEntry i_ring[MAX_I_RING_SIZE];
static uint64_t nr_i = 0; 

void push_inst(vaddr_t pc, const char *s) {
    ITraceEntry *e = &i_ring[nr_i % MAX_I_RING_SIZE];
    e->pc = pc;
    strncpy(e->asmb, s, sizeof(e->asmb) - 1);
    e->asmb[sizeof(e->asmb) - 1] = '\0';
    e->has_src = 0;
    nr_i++;
}
void print_recent_insts() {
    printf("------- [ Recent Instructions (itrace) ] -------\n");
    uint64_t start = (nr_i > MAX_I_RING_SIZE) ? (nr_i - MAX_I_RING_SIZE) : 0;

    for (uint64_t i = start; i < nr_i; i++) {
        ITraceEntry *e = &i_ring[i % MAX_I_RING_SIZE];
        
        char clean_asmb[64];
        strncpy(clean_asmb, e->asmb, 63);
        for (char *p = clean_asmb; *p; p++) if (*p == '\t') *p = ' ';

        char filename[128]; int line = -1;
        get_pc_source(e->pc, filename, &line);
        char *code = (line != -1) ? get_src(filename, line) : "";
        char *short_name = strrchr(filename, '/') ? strrchr(filename, '/') + 1 : filename;

        // 3. 重新设计格式化字符串
        // %-3s    : 箭头
        // %-40s   : 汇编（现在没有 Tab 了，对齐会很准）
        // | %s:%-3d : 文件和行号
        // | %s      : 源码
        printf("%s %-50s | %s:%-3d | %s", 
               (i == nr_i - 1 ? "-->" : "   "), 
               clean_asmb, 
               short_name, line, 
               code);
    }
}