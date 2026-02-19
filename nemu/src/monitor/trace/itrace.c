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

    int last_line = -1;
    char last_file[128] = "";

    for (uint64_t i = start; i < nr_i; i++) {
        ITraceEntry *e = &i_ring[i % MAX_I_RING_SIZE];
        
        char filename[128]; int line = -1;
        get_pc_source(e->pc, filename, &line);
        char *short_name = strrchr(filename, '/') ? strrchr(filename, '/') + 1 : filename;

        // 打印前半部分：箭头 + PC + 汇编
        printf("%s %-40s", (i == nr_i - 1 ? "-->" : "   "), e->asmb);

        // 判断是否需要打印源码列
        if (line != -1 && (line != last_line || strcmp(filename, last_file) != 0)) {
            char *code = get_src(filename, line);
            printf(" | %s:%-3d | %s", short_name, line, code);
            
            // 更新最后记录
            last_line = line;
            strcpy(last_file, filename);
        } else {
            // 如果行号相同，只打印分隔符或留空，保持对齐
            printf(" | %-10s |", ""); 
        }
        printf("\n");
    }
}