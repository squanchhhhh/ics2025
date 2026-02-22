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

static list_head ready_queue = LIST_HEAD_INIT(ready_queue);

PCB* pcb_alloc() {
  for (int i = 0; i < MAX_NR_PROC; i++) {
    if (pcb[i].state == UNUSED) {
      memset(&pcb[i], 0, sizeof(PCB)); 
      pcb[i].pid = i + 1; 
      for(int j = 0; j < MAX_NR_PROC_FILE; j++) pcb[i].fd_table[j] = -1;
      pcb[i].fd_table[0] = 0; // stdin
      pcb[i].fd_table[1] = 1; // stdout
      pcb[i].fd_table[2] = 2; // stderr
      return &pcb[i];
    }
  }
  return NULL; 
}

void pcb_enqueue(PCB *p) {
    if (p->state == READY) {
        list_add_tail(&p->list, &ready_queue);
    }
}

PCB* pcb_dequeue() {
    if (ready_queue.next == &ready_queue) return NULL;
    list_head *node = ready_queue.next;
    list_del(node);
    return list_entry(node, PCB, list);
}

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
    // 1. 确立当前身份为 idle (PID 0)
    current = &pcb_boot;
    current->pid = 0;
    strcpy(current->name, "idle");

    // 2. 加载第一个真正的人类进程 (PID 1)
    PCB *p1 = pcb_alloc(); // 找到 pcb[0]
    char * argv[] = {NULL};
    char * envp[] = {NULL};
    context_uload(p1, "/bin/nterm", argv, envp);
    p1->state = READY;
    pcb_enqueue(p1); // 加入链表
}
Context* schedule(Context *prev) {
  if (current != NULL) {
    current->cp = prev;

    if (current != &pcb_boot && current->state == RUNNING) {
      current->state = READY;
      pcb_enqueue(current);
    }
  }
  PCB *next = pcb_dequeue();

  if (next == NULL) {
    current = &pcb_boot;
  } else {
    current = next;
    current->state = RUNNING;
  }

  Log("Next process: %s (PID:%d)", current->name, current->pid);
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
  Log("Proc '%s' attempting to map fd, current fd_table[3]=%d", current->name, current->fd_table[3]);
  printf("Process %s: No available FD slots!\n", current->name);
  return -1;
}

void do_execve(const char *filename, char *const argv[], char *const envp[]) {
  context_uload(current, filename, argv, envp);
  yield(); 
}