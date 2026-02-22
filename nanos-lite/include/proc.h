#ifndef __PROC_H__
#define __PROC_H__

#include <common.h>
#include <memory.h>
#include <list.h>

#define STACK_SIZE (8 * PGSIZE)
#define MAX_NR_PROC_FILE 32
#define MAX_NR_PROC 4

// 进程状态枚举
typedef enum {
  UNUSED = 0, READY, RUNNING, BLOCKED, ZOMBIE
} pstate_t;

typedef struct pcb {
  Context *cp;        
  AddrSpace as;
  uintptr_t max_brk;
  
  int pid;
  pstate_t state;          
  char name[64];
  int exit_code;

  int fd_table[MAX_NR_PROC_FILE];
  
  list_head list;
  struct pcb * parent;
  uint8_t stack[STACK_SIZE] PG_ALIGN;
} PCB;

// 全局当前进程指针
extern PCB *current;

// --- 核心函数接口 ---

// 进程加载与切换
void init_proc();
Context* schedule(Context *prev);
void context_uload(PCB *pcb, const char *filename, char *const argv[], char *const envp[]);
void context_kload(PCB *pcb, void (*entry)(void *), void *arg);

// 进程生命周期管理
void do_execve(const char *filename, char *const argv[], char *const envp[]);
void do_exit(int status);
int do_brk(uintptr_t brk);

// 资源映射
int map_to_proc_fd(int s_idx);

#endif