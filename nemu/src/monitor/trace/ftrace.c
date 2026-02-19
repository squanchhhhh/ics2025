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
 * 从 start 位置开始，寻找当前函数调用对应的返回位置
 * 返回结束位置在 te 数组中的索引
 */
static int find_block_end(int start) {
  if (te[start].type != FUNC_CALL) return -1;
  
  int depth = 0;
  for (int i = start; i < nr_func_trace_event; i++) {
    if (te[i].func_id == te[start].func_id) {
      if (te[i].type == FUNC_CALL) depth++;
      else if (te[i].type == FUNC_RET) depth--;
      
      if (depth == 0) return i;
    }
  }
  return -1;
}
/**
 * 比较两个区间 [s1, e1] 和 [s2, e2] 的内容是否完全一致
 */
static bool is_block_equal(int s1, int e1, int s2, int e2) {
  if ((e1 - s1) != (e2 - s2)) return false;
  for (int i = 0; i <= (e1 - s1); i++) {
    if (te[s1 + i].func_id != te[s2 + i].func_id || 
        te[s1 + i].type != te[s2 + i].type) return false;
  }
  return true;
}
static void print_single_trace(FTraceEntry entry, int *indent) {
  Func *f = elf_get_func_by_id(entry.func_id);
  if (!f) return;

  printf("0x%08x: ", entry.pc);
  if (entry.type == FUNC_CALL) {
    for (int j = 0; j < *indent; j++) printf("  ");
    printf("call [%s@0x%08x]\n", f->name, f->begin);
    (*indent)++;
  } else {
    (*indent)--;
    if (*indent < 0) *indent = 0;
    for (int j = 0; j < *indent; j++) printf("  ");
    printf("ret  [%s]\n", f->name);
  }
}
void ftrace_print() {
  printf("------- [ FTrace Log ] -------\n");
  int indentation = 0;

  for (int i = 0; i < nr_func_trace_event; ) {
    int block_end = find_block_end(i);
    
    // 如果是 CALL 且找到了闭合的 RET
    if (te[i].type == FUNC_CALL && block_end != -1) {
      int block_size = block_end - i + 1;
      int repeat_count = 0;

      // 统计后面连续出现了多少个相同的块
      int next_s = block_end + 1;
      while (next_s + block_size <= nr_func_trace_event) {
        if (is_block_equal(i, block_end, next_s, next_s + block_size - 1)) {
          repeat_count++;
          next_s += block_size;
        } else break;
      }

      // --- 核心修改：完整输出第一个块 ---
      // 我们正常遍历第一个块内部的所有指令并打印
      int first_block_limit = block_end; 
      for (int k = i; k <= first_block_limit; k++) {
        print_single_trace(te[k], &indentation);
      }

      // 如果有重复，在第一个块结束后打印统计信息
      if (repeat_count > 0) {
        // 使用特殊格式标记折叠
        printf("      ... [folded %d identical sessions] ...\n", repeat_count);
      }

      // 直接跳过所有已打印和已折叠的记录
      i = next_s; 
    } 
    else {
      // 处理非嵌套或异常情况
      print_single_trace(te[i], &indentation);
      i++;
    }
  }
  printf("------------------------------\n");
}