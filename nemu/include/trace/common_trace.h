#include <common.h>

typedef enum { TRACE_INST, TRACE_MEM, TRACE_FUNC } TraceType;

typedef struct {
  uint32_t type; // TraceType
  vaddr_t pc;    // 发生的 PC
  union {
    struct { char content[64]; } inst;          // itrace: 汇编文本
    struct { paddr_t addr; uint64_t data; int len; bool is_write; } mem; // mtrace
    struct { vaddr_t target; int type; } func; // ftrace: call/ret
  } info;
} TraceEntry;