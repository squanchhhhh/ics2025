#include "am.h"
#include "debug.h"
#include <proc.h>
#include <stdio.h>
#include <string.h>
#include "fs.h"
#include "list.h"
#include "mm.h"
#define MAX_NR_PROC 4

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static PCB pcb_boot = {};
PCB *current = NULL;

static list_head ready_queue = LIST_HEAD_INIT(ready_queue);

PCB* pcb_alloc() {
  for (int i = 0; i < MAX_NR_PROC; i++) {
    if (pcb[i].state == UNUSED) {
      pcb[i].pid = i + 1;
      pcb[i].state = READY;
      INIT_LIST_HEAD(&pcb[i].list);
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
Context* copy_context_to_child_stack(PCB *child, Context *parent_ctx) {
  uintptr_t kstack_end = (uintptr_t)&child->stack + sizeof(child->stack);
  Context *child_ctx = (Context *)(kstack_end - sizeof(Context));
  
  // 1. 拷贝上下文
  memcpy(child_ctx, parent_ctx, sizeof(Context));
  
  // 2. 修正 pdir (必须包含 Sv32 模式位)
  uintptr_t mode = 1ul << (__riscv_xlen - 1);
  child_ctx->pdir = (void *)(mode | ((uintptr_t)child->as.ptr >> 12));
  
  return child_ctx;
}

int sys_fork(Context *c) {
    // 1. 先申请一个空的、有新 PID 的 PCB
    PCB *child = pcb_alloc();
    if (!child) return -1;

    // 2. 暂时保存子进程独有的属性
    int child_pid = child->pid;
    list_head child_list = child->list; 

    // 3. 拷贝父进程内容（注意这里会覆盖 pid 和 list）
    memcpy(child, current, sizeof(PCB));

    // 4. 还原/修正子进程独有的属性
    child->pid = child_pid;
    child->list = child_list; 
    child->parent = current; // 如果你在 PCB 里加了 parent 指针的话

    // 5. 内存拷贝 (这是你之前实现的 mm.c 里的函数)
    copy_as(&child->as, &current->as);
    
    // 6. 构造上下文
    child->cp = copy_context_to_child_stack(child, c);
    child->cp->GPRx = 0; // 子进程 a0 = 0

    // 7. 加入调度队列
    child->state = READY;
    pcb_enqueue(child);

    return child->pid; // 父进程拿到子进程 PID
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
    context_uload(p1, "/bin/hello", argv, envp);
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