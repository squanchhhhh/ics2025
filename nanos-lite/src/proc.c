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
  while (1) {
    if (j % 100 == 0){
          Log("Hello World from Nanos-lite with arg '%p' for the %dth time!", (void *)(uintptr_t)arg, j);
    }
    j ++;
    if (j == 5000){
      halt(0);
    }
    yield();
  }
}
void hello_fun_another(void *arg) {
  int j = 1;
  while (1) {
    if (j % 100 == 0) {
      Log("Greetings from the SECOND thread! arg: '%p', count: %d", arg, j);
    }
    j++;
    yield();
  }
}

void init_proc() {
  // 初始化第一个内核线程
  //context_kload(&pcb[0], hello_fun, (void *)1);
  
  // 初始化第二个内核线程
  // context_kload(&pcb[1], hello_fun_another, (void *)2);

  switch_boot_pcb();

  //char *argv[] = {"hello", "world", NULL};
  //char *envp[] = {"PATH=/bin:/usr/bin", NULL};
  char *argv[] = {NULL};
  char *envp[] = {NULL};
  context_uload(&pcb[1], "/bin/hello", argv, envp);

  switch_boot_pcb();
}

Context* schedule(Context *prev) {
  current->cp = prev;
  
  PCB *old = current;
  if (current == &pcb[0]) {
    current = &pcb[0];
  } else {
    current = &pcb[0];
  }

  // 增加这行 Log
  printf("[Sched] %s -> %s (EPC: %x)\n", 
         (old == &pcb_boot ? "boot" : (old == &pcb[0] ? "pcb0" : "pcb1")),
         (current == &pcb[0] ? "pcb0" : "pcb1"),
         current->cp->mepc);

  return current->cp;
}
/*
Context* schedule(Context *prev) {
  printf("call schedule\n");
  current->cp = prev;
  
  if (current == &pcb[0]) {
    current = &pcb[1];
  } else {
    current = &pcb[0];
  }
  
  // 打印确认
  printf("Next PCB: %p, pdir: %p\n", current, current->cp->pdir);
  printf("[Debug] Target SP in Context: %p\n", (void *)current->cp->gpr[2]);
  return current->cp;
}
*/

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