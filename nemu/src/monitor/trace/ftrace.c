#include "trace/ftrace.h"
#define FUNC_NUM 128
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