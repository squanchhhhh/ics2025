#ifndef __FTRACE_H__
#define __FTRACE_H__

#include <common.h>

#define FUNC_NUM 128

typedef struct {
  bool valid;
  char name[32];
  vaddr_t begin;
  vaddr_t end;
} Func;

extern Func funcs[FUNC_NUM];
extern int nr_func;

int find_func_by_addr(vaddr_t addr);

#endif