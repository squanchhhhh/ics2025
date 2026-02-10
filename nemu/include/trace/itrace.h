#ifndef __ITRACE_H__
#define __ITRACE_H__

#include "common.h"

#define RING_SIZE 10
#define LOG_BUF_LEN 128

typedef struct {
  vaddr_t pc[RING_SIZE]; 
  char buf[RING_SIZE][LOG_BUF_LEN];
  
  int tail;      // 指向下一个写入位置
  int error_idx; // 记录出错指令的索引
  int num;       // 记录当前缓冲区中有效的指令条数
} IRing;

extern IRing iringbuf;
void push_inst(vaddr_t pc, const char *s);
void print_recent_insts();
void init_iring();

#endif