#include <trace/ftrace.h>
#include <trace/elf.h>
#include <trace/tui.h>

static FTraceEntry te[MAX_FUNC_TRACE];
static int nr_func_trace_event = 0;

/**
 * 记录追踪事件
 */
void ftrace_record(vaddr_t caller_pc, vaddr_t target_addr, FTraceType type) {
  if (!is_elf_loaded()) return;
  if (nr_func_trace_event >= MAX_FUNC_TRACE) {
    static bool warned = false;
    if (!warned) {
      Log("ftrace: trace event buffer is full!");
      warned = true;
    }
    return;
  }
  int fid = elf_find_func_by_addr(target_addr);
  if (fid == -1) {
    return; 
  }
  te[nr_func_trace_event].pc = caller_pc;
  te[nr_func_trace_event].func_id = fid;
  te[nr_func_trace_event].type = type;
  nr_func_trace_event++;
}

/**
 * 格式化打印追踪记录
 */
void ftrace_print() {
  printf("------- [ FTrace Log ] -------\n");
  int indentation = 0;

  char filename[256];
  int line_num;

  for (int i = 0; i < nr_func_trace_event; i++) {
    Func *f = elf_get_func_by_id(te[i].func_id);
    if (!f) continue;

    // 1. 获取当前 PC 对应的源码位置
    get_pc_source(te[i].pc, filename, &line_num);

    // 2. 打印 PC 地址和源码信息（用括号标注）
    printf("0x%08x [%s:%d]: ", te[i].pc, filename, line_num);

    if (te[i].type == FUNC_CALL) {
      for (int j = 0; j < indentation; j++) printf("  ");
      printf("call [%s@0x%08x]\n", f->name, f->begin);
      indentation++;
    } 
    else if (te[i].type == FUNC_RET) {
      indentation--;
      if (indentation < 0) indentation = 0;
      for (int j = 0; j < indentation; j++) printf("  ");
      printf("ret  [%s]\n", f->name);
    }
  }
  printf("------------------------------\n");
}