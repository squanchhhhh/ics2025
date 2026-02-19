#ifndef __ITRACE_H__
#define __ITRACE_H__

#include "common.h"

typedef struct {
    vaddr_t pc;
    char asmb[64];        // 汇编文本
    char filename[128];   // 文件名
    int line;             // 行号
    bool has_src;         // 是否成功获取了源码信息
} ITraceEntry;

void push_inst(vaddr_t pc, const char *s);
void print_recent_insts();

#endif