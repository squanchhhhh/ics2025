#ifndef __MTRACE_H__
#define __MTRACE_H__

#include <common.h>
#endif


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
typedef struct {
    MTraceEntry buf[MTRACE_BUF_SIZE];
    int tail;
    int num;      
} MTraceBuffer;
void dump_mtrace(void) ;