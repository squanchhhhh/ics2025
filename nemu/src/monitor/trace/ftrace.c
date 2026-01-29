#include "trace/ftrace.h"

Func funcs[FUNC_NUM];
int nr_func = 0;

int find_func_by_addr(vaddr_t addr) {
  for (int i = 0; i < nr_func; i++) {
    if (funcs[i].valid &&
        addr >= funcs[i].begin &&
        addr <  funcs[i].end) {
      return i;
    }
  }
  return -1;
}

void init_funcs() {
  for (int i = 0; i < FUNC_NUM; i++) {
    funcs[i].valid = 0;
  }
  return;
}

FTraceEntry te[MAX_FUNC_TRACE];
int nr_func_trace_event = 0;
//函数调用记录
void ftrace_record(vaddr_t caller_pc,int fid,FTraceType type){
  te[nr_func_trace_event].type = type;
  te[nr_func_trace_event].func_id = fid;
  te[nr_func_trace_event].pc = caller_pc;
  nr_func_trace_event++;
  return ;
}
//函数调用跟踪
void ftrace_print() {
  int indentation = 0;
  for (int i = 0; i < nr_func_trace_event; i++) {
    Func f = funcs[te[i].func_id];
    printf("%x:", te[i].pc);
    if (te[i].type == FUNC_CALL) {
      for (int j = 0; j < indentation; j++) {
        printf(" ");
      }
      printf("call [%s@%x]\n", f.name, f.begin);
      indentation++;
    }
    else if (te[i].type == FUNC_RET) {
      indentation--;
      if (indentation < 0) indentation = 0; 
      for (int j = 0; j < indentation; j++) {
        printf(" ");
      }
      printf("ret [%s]\n", f.name);
    }
  }
}