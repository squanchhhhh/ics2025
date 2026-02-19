#include "trace/itrace.h"
#include "trace/tui.h"
#include <string.h>

#define MAX_ITRACE 16
static ITraceEntry ie[MAX_ITRACE];
static uint64_t nr_i = 0; 

void push_inst(vaddr_t pc, const char *s) {
    ITraceEntry *e = &ie[nr_i % MAX_ITRACE];
    e->pc = pc;
    strncpy(e->asmb, s, sizeof(e->asmb) - 1);
    e->asmb[sizeof(e->asmb) - 1] = '\0';
    e->has_src = 0;
    nr_i++;
}

void print_recent_insts() {
    printf("------- [ Recent Instructions (itrace) ] -------\n");
    uint64_t start = (nr_i > MAX_ITRACE) ? (nr_i - MAX_ITRACE) : 0;

    for (uint64_t i = start; i < nr_i; i++) {
        ITraceEntry *e = &ie[i % MAX_ITRACE];
        
        char clean_asmb[64];
        strncpy(clean_asmb, e->asmb, 63);
        for (char *p = clean_asmb; *p; p++) if (*p == '\t') *p = ' ';

        char filename[128]; int line = -1;
        get_pc_source(e->pc, filename, &line);
        char *code = (line != -1) ? get_src(filename, line) : "";
        char *short_name = strrchr(filename, '/') ? strrchr(filename, '/') + 1 : filename;

        printf("%s %-50s | %s:%-5d | %s", 
               (i == nr_i - 1 ? "-->" : "   "), 
               clean_asmb, 
               short_name, line, 
               code);
    }
}