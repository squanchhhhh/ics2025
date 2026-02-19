#ifndef __MTRACE_H__
#define __MTRACE_H__

#include <common.h>

#define MTRACE_BUF_SIZE 32
typedef enum {
    MEM_READ,
    MEM_WRITE
} MemAccessType;

typedef struct {
    vaddr_t pc;          // PC
    paddr_t addr;        // 地址
    uint64_t data;       // 数据
    int len;             // 长度（1/2/4/8）
    MemAccessType type;  // 读 / 写
} MTraceEntry;

void dump_mtrace(void) ;
void push_mtrace(vaddr_t pc,paddr_t addr,uint64_t data,int len,MemAccessType type);
#endif