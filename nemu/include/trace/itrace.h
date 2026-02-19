#ifndef __ITRACE_H__
#define __ITRACE_H__

#include "common.h"

typedef struct {
    int reg_num;  
    word_t old_val;
    word_t new_val;
} RegEntry;

typedef struct {
    vaddr_t pc;
    char asmb[64]; 
    RegEntry reg; 
} ITraceEntry;


void push_inst(vaddr_t pc, const char *s, RegEntry *reg);
void dump_insts();

#endif