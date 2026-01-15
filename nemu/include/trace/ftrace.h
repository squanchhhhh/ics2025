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

typedef enum { TRACE_CALL, TRACE_RET } trace_type;
int find_func_by_addr(vaddr_t addr);
void ftrace_record(vaddr_t caller_pc,int fid,trace_type type);
#define MAX_TRACE_EVENT 4096

typedef struct {
  trace_type type;
  vaddr_t pc; // 调用地址
  int func_id;
} TraceEvent;
extern TraceEvent te[MAX_TRACE_EVENT];
extern int nr_trace_event;
void ftrace_print();
#endif