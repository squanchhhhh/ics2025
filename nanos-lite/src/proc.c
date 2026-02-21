#include "am.h"
#include "debug.h"
#include <proc.h>
#include <stdio.h>
#include <string.h>
#include "fs.h"
#define MAX_NR_PROC 4

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static PCB pcb_boot = {};
PCB *current = NULL;

void switch_boot_pcb() {
  current = &pcb_boot;
}

void hello_fun(void *arg) {
  int j = 1;
  if (j == 1){
    printf("first time in hello_fun\n");
  }
  while (j<10) {
    Log("Hello World from Nanos-lite with arg '%p' for the %dth time!", (void *)(uintptr_t)arg, j);
    j ++;
    yield();
  }
}
void hello_fun_another(void *arg) {
  int j = 1;
  if (j == 1){
    printf("first time in hello_fun_another\n");
  }
  while (j<10) {
    Log("Greetings from the SECOND thread! arg: '%p', count: %d", arg, j);
    j++;
    yield();
  }
}

void init_proc() {
  // 初始化第一个内核线程
  context_kload(&pcb[0], hello_fun, (void *)1);
  
  // 初始化第二个内核线程
  context_kload(&pcb[1], hello_fun_another, (void *)2);

  switch_boot_pcb();

  //char *argv[] = {"hello", "world", NULL};
  //char *envp[] = {"PATH=/bin:/usr/bin", NULL};
  char *argv[] = {NULL};
  char *envp[] = {NULL};
  context_uload(&pcb[2], "/bin/hello", argv, envp);

  switch_boot_pcb();
}

Context* schedule(Context *prev) {
  printf("proc schedule\n");
  // 1. 记录切换前的状态
  //int old_idx = (current == &pcb[0] ? 0 : 1); // 假设你目前主要在跑前两个
  
  // 2. 保存当前现场
  current->cp = prev;

  // 3. 核心逻辑：选择下一个进程
  static int pcb_idx = 0;
  pcb_idx = (pcb_idx + 1) % 3; // 先固定在 pcb0 和 pcb1 之间切，方便调试
  current = &pcb[pcb_idx];

  // 如果目标进程没初始化，强制切回 pcb0
  if (current->cp == NULL) {
    pcb_idx = 0;
    current = &pcb[0];
  }

  // 4. 打印极其详细的日志
  // %p 打印地址，方便比对是否踩到了 PCB 头部
  //printf("[Sched] %d->%d | prev_cp:%p | next_cp:%p | next_EPC:%x | next_PDIR:%p | &current->cp->pdir = %p\n",
         //prev_idx, pcb_idx, prev, current->cp, current->cp->mepc, current->cp->pdir,&current->cp->pdir);

  // 5. 增加一个紧急防御检测（选做）
  // 如果 cp 指向的位置就在 pcb 结构体的开头，说明它极大概率被改写了
  if ((uintptr_t)current->cp == (uintptr_t)current) {
    printf("  WARNING: cp is pointing to PCB head! Potential overwrite detected.\n");
  }

  return current->cp;
}

/*
功能：在当前进程的文件描述符中，添加一个系统打开文件表的对应
1.寻找一个空的文件描述符表项
2.填入系统打开文件表的下标
其中，0-2号文件描述符为打开进程时系统设置为stdio
*/
int map_to_proc_fd(int s_idx) {
  if (s_idx < 0 || s_idx >= MAX_OPEN_FILES) {
    panic("s_idx %d is invalid\n",s_idx);
  }
  for (int i = 3; i < MAX_NR_PROC_FILE; i++) {
    if (current->fd_table[i] == -1) { 
      current->fd_table[i] = s_idx;
      // Log("FD_ALLOC: proc '%s' assigned fd %d to sys_idx %d", current->name, i, s_idx);
      return i;
    }
  }
  printf("Process %s: No available FD slots!\n", current->name);
  return -1;
}

void do_execve(const char *filename, char *const argv[], char *const envp[]) {
  context_uload(current, filename, argv, envp);
  yield(); 
}