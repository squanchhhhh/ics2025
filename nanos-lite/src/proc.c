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
  while (1) {
    if (j % 100 == 0){
          Log("Hello World from Nanos-lite with arg '%p' for the %dth time!", (void *)(uintptr_t)arg, j);
    }
    j ++;
    yield();
  }
}

void init_proc() {
  context_kload(&pcb[0], hello_fun, (void *)1);

  //char *argv[] = {"hello", "world", NULL};
  //char *envp[] = {"PATH=/bin:/usr/bin", NULL};
  char *argv[] = {NULL};
  char *envp[] = {NULL};
  context_uload(&pcb[1], "/bin/hello", argv, envp);

  switch_boot_pcb();
}

Context* schedule(Context *prev) {
  // 1. 保存现场
  current->cp = prev;

  // 2. 切换到用户进程
  current = &pcb[1];

  // 3. 输出进入用户程序前的全部关键内核信息
  printf("\n--- [KERNEL PANOPTICON: JUMPING TO USER] ---\n");
  printf("Target PCB      : %p (pcb[1])\n", current);
  printf("Target Context  : %p (on kstack)\n", current->cp);
  
  // 核心寄存器信息
  printf("MEPC (PC)       : 0x%08x\n", current->cp->mepc);
  printf("MSTATUS         : 0x%08x\n", current->cp->mstatus);
  printf("User SP (gpr[2]): 0x%08x\n", current->cp->gpr[2]);
  printf("Arg0 (a0/gpr[10]): 0x%08x\n", current->cp->gpr[10]);
  
  // 内存/页表信息
  printf("Page Table (SATP): %p\n", current->cp->pdir);
  if (current->cp->pdir) {
      // 这里的 as 是 PCB 中的物理地址空间描述符
      printf("AS Area Start   : %p\n", current->as.area.start);
      printf("AS Area End     : %p\n", current->as.area.end);
  }
  
  // 检查是否发生了栈重叠或污染
  uintptr_t kstack_top = (uintptr_t)current + sizeof(PCB);
  printf("Current KStack  : Top=%p, CurrentCP=%p, SpaceLeft=%d bytes\n", 
          (void *)kstack_top, current->cp, (int)((uintptr_t)current->cp - (uintptr_t)current));
  
  printf("--------------------------------------------\n\n");

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