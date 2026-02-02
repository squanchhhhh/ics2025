#include <trace/ftrace.h>
#include <trace/elf.h>

static FTraceEntry te[MAX_FUNC_TRACE];
static int nr_func_trace_event = 0;

/**
 * 记录追踪事件
 */
void ftrace_record(vaddr_t caller_pc, vaddr_t target_addr, FTraceType type) {
  // 1. 如果没加载 ELF，记录也没有意义
  if (!is_elf_loaded()) return;

  // 2. 检查缓冲区是否溢出
  if (nr_func_trace_event >= MAX_FUNC_TRACE) {
    static bool warned = false;
    if (!warned) {
      Log("ftrace: trace event buffer is full!");
      warned = true;
    }
    return;
  }

  // 3. 通过 elf 模块查询目标地址所属的函数 ID
  int fid = elf_find_func_by_addr(target_addr);
  if (fid == -1) {
    // 找不到符号可能是跳到了非函数区域，或者符号表不全
    return; 
  }

  // 4. 写入记录
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