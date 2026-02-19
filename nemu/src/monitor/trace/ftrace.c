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
    // 1. 寻找当前函数块的结束位置 (对应的 RET)
    int block_end = -1;
    if (te[i].type == FUNC_CALL) {
      int depth = 0;
      for (int j = i; j < nr_func_trace_event; j++) {
        if (te[j].type == FUNC_CALL && te[j].func_id == te[i].func_id) depth++;
        if (te[j].type == FUNC_RET && te[j].func_id == te[i].func_id) {
          depth--;
          if (depth == 0) { block_end = j; break; }
        }
      }
    }

    // 2. 检查后续是否有完全相同的块
    int repeat_count = 0;
    if (block_end != -1) {
      int block_size = block_end - i + 1;
      while (block_end + block_size < nr_func_trace_event) {
        bool match = true;
        for (int k = 0; k < block_size; k++) {
          if (te[i + k].func_id != te[block_end + 1 + k].func_id ||
              te[i + k].type != te[block_end + 1 + k].type) {
            match = false;
            break;
          }
        }
        if (match) {
          repeat_count++;
          block_end += block_size; // 移动到下一个重复块的末尾
        } else {
          break;
        }
      }
    }

    // 3. 打印当前项
    Func *f = elf_get_func_by_id(te[i].func_id);
    if (!f) continue;

    printf("0x%08x: ", te[i].pc);
    if (te[i].type == FUNC_CALL) {
      for (int j = 0; j < indentation; j++) printf("  ");
      if (repeat_count > 0) {
        printf("call [%s@0x%08x] (folded %d times)\n", f->name, f->begin, repeat_count + 1);
        // 如果折叠了，直接跳过整个块的所有后续记录（包括里面的子调用）
        i = block_end; 
        // 注意：因为我们跳过了整个块，不需要维护缩进，因为逻辑上我们还在当前层
      } else {
        printf("call [%s@0x%08x]\n", f->name, f->begin);
        indentation++;
      }
    } else {
      indentation--;
      if (indentation < 0) indentation = 0;
      for (int j = 0; j < indentation; j++) printf("  ");
      printf("ret  [%s]\n", f->name);
    }
  }
  printf("------------------------------\n");
}