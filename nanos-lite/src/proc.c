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
    yield();
  }
}
void hello_fun_another(void *arg) {
  int j = 1;
  if (j == 1){
    printf("first time in hello_fun_another\n");
  }
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
  // 1. 将当前的上下文指针保存回当前进程的 PCB 中
  current->cp = prev;

  // 2. 查找下一个准备就绪的进程
  // 这里使用静态变量记录位置，实现公平轮询
  static int pcb_idx = 0;
  
  // 简单的轮询逻辑：尝试寻找下一个 PCB
  // 如果你后续给 PCB 增加了 'status' 字段（如 RUNNING/ZOMBIE），
  // 可以在这里通过循环跳过那些没准备好的进程
  pcb_idx = (pcb_idx + 1) % MAX_NR_PROC;

  // 3. 切换指针
  current = &pcb[pcb_idx];

  // 4. 防御性检查：确保我们要切入的进程确实被加载了
  if (current->cp == NULL) {
    // 如果切到了一个还没初始化的 PCB，通常切回 pcb[0]（内核空闲进程）
    pcb_idx = 0;
    current = &pcb[0];
  }

   //5. 打印调度日志（调试用）
  printf("[Sched] To pcb%d (EPC: %x, PDIR: %p)\n", 
           pcb_idx, current->cp->mepc, current->cp->pdir);

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