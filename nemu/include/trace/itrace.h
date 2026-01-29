#ifndef __ITRACE_H__
#define __ITRACE_H__


#include "common.h"
//指令缓冲区

#define RING_SIZE 10
#define LOG_BUF_LEN 128

typedef struct{
  char buf[RING_SIZE][LOG_BUF_LEN];
  int tail;
  int error_idx;
  int  num;
}IRing;
extern IRing iringbuf;
void push_inst(const char * s);
void print_recent_insts();
void init_iring();
#endif