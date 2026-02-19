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
    uint64_t end = nr_i;

    for (uint64_t i = start; i < end; i++) {
        ITraceEntry *e = &i_ring[i % MAX_I_RING_SIZE];
        
        const char *prefix = (i == nr_i - 1) ? "-->" : "   ";
        printf("%s 0x%08x: %-40s", prefix, e->pc, e->asmb);

        char filename[256];
        int line_num = -1;
        get_pc_source(e->pc, filename, &line_num);

        if (line_num != -1) {
            char *content = get_src(filename, line_num);
            char *short_name = strrchr(filename, '/');
            short_name = (short_name) ? short_name + 1 : filename;

            printf(" | %s:%d | %s", short_name, line_num, content);
        } else {
            printf(" | [unknown source]");
        }
        printf("\n");
    }
    printf("------------------------------------------------\n");
}