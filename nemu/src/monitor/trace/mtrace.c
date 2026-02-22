#include "trace/mtrace.h"
#include "trace/tui.h"
#include "trace/error.h"
#include <assert.h>
#define MAX_MTRACE 16
MTraceEntry me[MAX_MTRACE];
static uint64_t nr_m = 0; 

void push_m(vaddr_t pc, paddr_t addr, uint64_t data, int len, MemAccessType type) {
    if (addr == pc && type == MEM_READ) {
        return;
    }
    /*
    if (type == MEM_WRITE && (uint32_t)data == 0x80519000) {
        printf("\n\033[1;31m[MTRACE BINARY TRAP]\033[0m Detected write of corrupted pdir value!\n");
        printf("  PC   : 0x%08x\n", pc);
        printf("  Addr : 0x%08x (This is where c->pdir is located)\n", addr);
        printf("  Data : 0x%016lx\n", data);
        
        // 尝试获取源码位置
        char file[128]; int line;
        get_pc_source(pc, file, &line);
        printf("  Source: %s:%d\n", file, line);
        
        // 触发自毁/停机，方便你直接看 trace
        assert(0); 
    }
        */
    MTraceEntry *e = &me[nr_m % MAX_MTRACE];
    e->addr = addr;
    e->data = data;
    e->len  = len;
    e->pc   = pc;
    e->type = type;
    nr_m++; 
}

void dump_mtrace(void) {
    printf("------- [ Recent Memory Trace (mtrace) ] -------\n");
    uint64_t start = (nr_m > MAX_MTRACE) ? (nr_m - MAX_MTRACE) : 0;

    for (uint64_t i = start; i < nr_m; i++) {
        MTraceEntry *e = &me[i % MAX_MTRACE];
        
        // 1. 获取源码位置（惰性求值）
        char file[128]; int line;
        get_pc_source(e->pc, file, &line);
        char *short_name = strrchr(file, '/') ? strrchr(file, '/') + 1 : file;

        // 2. 格式化输出
        // 使用颜色或符号区分读写
        const char *op = (e->type == MEM_READ) ? "  READ " : " \033[1;31mWRITE\033[0m"; 
        
        printf("%s  pc:0x%08x  addr:0x%08x  len:%d  data:0x%016lx | %s:%d\n", 
               op, e->pc, e->addr, e->len, e->data, short_name, line);
    }
}

void get_error_mtrace(ErrorEntry *e) {
    memset(&e->me, 0, sizeof(MTraceEntry));
    e->me.pc = 0; // 用 PC=0 表示未关联到内存记录

    if (nr_m == 0) return;

    uint64_t count = (nr_m > MAX_MTRACE) ? MAX_MTRACE : nr_m;
    
    for (uint64_t i = 0; i < count; i++) {
        uint64_t idx = (nr_m - 1 - i) % MAX_MTRACE;
        if (me[idx].pc == e->ie.pc) {
            e->me = me[idx];
            return;
        }
    }
}