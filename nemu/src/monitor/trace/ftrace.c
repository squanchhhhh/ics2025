#include <trace/ftrace.h>
#include <trace/elf.h>

#define MAX_FUNC_TRACE 1024
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

  for (int i = 0; i < nr_func_trace_event; i++) {
    Func *f = elf_get_func_by_id(te[i].func_id);
    if (!f) continue;

    // 打印当前的 PC 地址
    printf("0x%08x: ", te[i].pc);

    if (te[i].type == FUNC_CALL) {
      // 处理缩进
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

void ftrace_print_stack() {
  printf("------- [ FTrace Stack ] -------\n");

  int call_stack[MAX_FUNC_TRACE];
  int sp = 0;

  for (int i = 0; i < nr_func_trace_event; i++) {
    if (te[i].type == FUNC_CALL) {
      call_stack[sp++] = te[i].func_id;
    } else if (te[i].type == FUNC_RET) {
      if (sp > 0) sp--;
    }
  }

  // 按 GDB 风格输出：#0 是当前函数
  for (int i = sp - 1, frame = 0; i >= 0; i--, frame++) {
    Func *f = elf_get_func_by_id(call_stack[i]);
    if (!f) continue;
    printf("#%d %s\n", frame, f->name);
  }

  printf("-------------------------------\n");
}

/**
 * 根据 PC 返回函数名和偏移量，例如 "main+12"
 * 用于 ErrorLog 的横向整合输出
 */
char* get_f_name(vaddr_t pc) {
    static char func_info[128];
    int fid = elf_find_func_by_addr(pc);
    if (fid != -1) {
        Func *f = elf_get_func_by_id(fid);
        uint32_t offset = pc - f->begin;
        if (offset == 0) {
            snprintf(func_info, sizeof(func_info), "<%s+0x%x>", f->name, offset);
        } else {
            snprintf(func_info, sizeof(func_info), "<%s+0x%x>", f->name, offset);
        }
        return func_info;
    }
    return NULL; 
}