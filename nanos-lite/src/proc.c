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
    Log("Hello World from Nanos-lite with arg '%p' for the %dth time!", (void *)(uintptr_t)arg, j);
    j ++;
    yield();
  }
}
/*
void init_proc() {
  switch_boot_pcb();
  Log("Initializing processes...");
  char * file_name = "/bin/nterm";
  current->name = file_name;
  for (int i = 0; i < MAX_NR_PROC_FILE; i++) {
      if (i < 3) {
        current->fd_table[i] = i;
      } else {
        current->fd_table[i] = -1;
      }
    }
  naive_uload(NULL, file_name);
}*/
void init_proc() {
  Area stack0 = { .start = pcb[0].stack, .end = pcb[0].stack + sizeof(pcb[0].stack) };
  pcb[0].cp = kcontext(stack0, hello_fun, (void *)1);

  Area stack1 = { .start = pcb[1].stack, .end = pcb[1].stack + sizeof(pcb[1].stack) };
  pcb[1].cp = kcontext(stack1, hello_fun, (void *)2);

  current = &pcb[0];

  Log("Initializing processes... Two kernel threads created.");

}

Context* schedule(Context *prev) {
  current->cp = prev;
  current = (current == &pcb[0] ? &pcb[1] : &pcb[0]);
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

void do_execve(const char *filename) {
    naive_uload(NULL, filename);
}