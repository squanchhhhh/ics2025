#include "trace/mtrace.h"

#define MAX_MTRACE 16
MTraceEntry me[MAX_MTRACE];
static uint64_t nr_m = 0; 

void push_mtrace(vaddr_t pc, paddr_t addr, uint64_t data, int len, MemAccessType type) {
    if (addr == pc && type == MEM_READ) {
        return;
    }

    MTraceEntry *e = &me[nr_m % MAX_MTRACE];
    
    e->addr = addr;
    e->data = data;
    e->len  = len;
    e->pc   = pc;
    e->type = type;

    nr_m++; 
}
void dump_mtrace(void) {
    if (nr_m == 0) {
        printf("No memory trace records.\n");
        return;
    }

    uint64_t start = (nr_m > MAX_MTRACE) ? (nr_m - MAX_MTRACE) : 0;

    printf("------- [ Recent Memory Trace (mtrace) ] -------\n");
    for (uint64_t i = start; i < nr_m; i++) {
        MTraceEntry *e = &me[i % MAX_MTRACE];
        
        printf("  %s  pc=0x%08x  addr=0x%08x  len=%d  data(hex)=0x%016lx\n", 
               (e->type == MEM_READ) ? "READ " : "WRITE",
               e->pc, 
               e->addr, 
               e->len, 
               e->data);
    }
    printf("------------------------------------------------\n");
}