#ifndef __FTRACE_H__
#define __FTRACE_H__

#include <common.h>

// 追踪事件类型
typedef enum {
  FUNC_CALL,
  FUNC_RET
} FTraceType;

// 单条追踪记录
typedef struct {
  vaddr_t pc;       // 发起调用的指令地址 (caller_pc)
  int func_id;      // 目标函数在 elf 模块中的 ID
  FTraceType type;  // 事件类型：call 或 ret
} FTraceEntry;

#define MAX_FUNC_TRACE 8192

// 记录一次函数调用或返回
// target_addr: call 指令的目标地址（用于查询函数名）
// caller_pc:   当前 call 指令本身的地址（用于打印显示）
void ftrace_record(vaddr_t caller_pc, vaddr_t target_addr, FTraceType type);

// 打印所有追踪记录
void ftrace_print();

#endif // __FTRACE_H__