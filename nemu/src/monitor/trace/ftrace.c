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
  int repeat_count = 0;

  for (int i = 0; i < nr_func_trace_event; i++) {
    // 检查是否与下一条记录重复 (类型、函数ID均一致)
    bool is_repeated = false;
    if (i + 1 < nr_func_trace_event) {
      if (te[i].type == te[i+1].type && te[i].func_id == te[i+1].func_id) {
        is_repeated = true;
      }
    }

    if (is_repeated) {
      repeat_count++;
      // 如果是 CALL，依然要维护缩进，但不打印
      if (te[i].type == FUNC_CALL) indentation++;
      else if (te[i].type == FUNC_RET) { indentation--; if(indentation<0) indentation=0; }
      continue; // 跳过本次循环，进入累加
    }

    // --- 开始打印 ---
    Func *f = elf_get_func_by_id(te[i].func_id);
    if (!f) continue;

    printf("0x%08x: ", te[i].pc);

    if (te[i].type == FUNC_CALL) {
      for (int j = 0; j < indentation; j++) printf("  ");
      
      if (repeat_count > 0) {
        printf("call [%s@0x%08x] * %d\n", f->name, f->begin, repeat_count + 1);
      } else {
        printf("call [%s@0x%08x]\n", f->name, f->begin);
      }
      
      indentation++;
    } 
    else if (te[i].type == FUNC_RET) {
      indentation--;
      if (indentation < 0) indentation = 0;
      for (int j = 0; j < indentation; j++) printf("  ");

      if (repeat_count > 0) {
        printf("ret  [%s] * %d\n", f->name, repeat_count + 1);
      } else {
        printf("ret  [%s]\n", f->name);
      }
    }

    // 打印完后重置计数器
    repeat_count = 0;
  }
  printf("------------------------------\n");
}