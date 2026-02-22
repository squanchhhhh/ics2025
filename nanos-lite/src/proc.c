#include "am.h"
#include "common.h"
#include "debug.h"
#include <proc.h>
#include <stdio.h>
#include <string.h>
#include "fs.h"
#include "list.h"
#include "mm.h"
#define PROC_LOG 1
#define MAX_NR_PROC 4

static PCB pcb[MAX_NR_PROC] __attribute__((used)) = {};
static PCB pcb_boot = {};
PCB *current = NULL;

static list_head ready_queue = LIST_HEAD_INIT(ready_queue);


PCB* pcb_alloc() {
  MLOG(PROC_LOG,"call pcb_alloc current = %s",current->name);
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
  memcpy(child_ctx, parent_ctx, sizeof(Context));
  uintptr_t mode = 1ul << (__riscv_xlen - 1);
  child_ctx->pdir = (void *)(mode | ((uintptr_t)child->as.ptr >> 12));
  return child_ctx;
}

int sys_fork(Context *c) {
    MLOG(PROC_LOG, "Step 1: Allocating PCB...");
    PCB *child = pcb_alloc();
    if (!child) return -1;

    int child_pid = child->pid;
    list_head child_list = child->list; 

    MLOG(PROC_LOG, "Step 2: Cloning PCB...");
    memcpy(child, current, sizeof(PCB));
    child->pid = child_pid;
    child->list = child_list; 
    child->parent = current; 

    MLOG(PROC_LOG, "Step 3: Copying Address Space...");
    copy_as(&child->as, &current->as);

    MLOG(PROC_LOG, "Step 4: Setting up context...");
    child->cp = copy_context_to_child_stack(child, c);
    child->cp->GPRx = 0; 
    
    child->state = READY;
    pcb_enqueue(child);

    MLOG(PROC_LOG, "Step 5: Fork Done, returning %d", child->pid);
    return child->pid; 
}
/**
 * sys_wait - 令当前进程进入阻塞状态，直到任一子进程结束
 * 返回值: 结束的子进程 PID，若无子进程则返回 -1
 */
int sys_wait(int *status) {
    MLOG(PROC_LOG, "PID %d (%s) is waiting for children", current->pid, current->name);
    bool has_child = false;
    while (1) {
        for (int i = 0; i < MAX_NR_PROC; i++) {
            if (pcb[i].parent == current && pcb[i].state != UNUSED) {
                has_child = true;
                if (pcb[i].state == ZOMBIE) {
                    int child_pid = pcb[i].pid;
                    MLOG(PROC_LOG, "Parent PID %d found Zombie child PID %d, reclaiming...", current->pid, child_pid);
                    unprotect(&pcb[i].as);
                    pcb[i].state = UNUSED;
                    
                    return child_pid;
                }
            }
        }
        if (!has_child) {
            MLOG(PROC_LOG, "PID %d has no children to wait for", current->pid);
            return -1;
        }
        current->state = BLOCKED;
        MLOG(PROC_LOG, "No Zombie child found, PID %d entering BLOCKED state", current->pid);
        yield(); 
    }
}


void do_exit(int status) {
    MLOG(PROC_LOG, "Process PID %d (%s) exiting with status %d", current->pid, current->name, status);
    current->state = ZOMBIE;
    if (current->parent && current->parent->state == BLOCKED) {
        MLOG(PROC_LOG, "Wake up parent PID %d", current->parent->pid);
        current->parent->state = READY;
        pcb_enqueue(current->parent);
    }
    yield();
}

void do_execve(const char *filename, char *const argv[], char *const envp[]) {
    MLOG(PROC_LOG, "Execve: PID %d replacing image with %s", current->pid, filename);
    context_uload(current, filename, argv, envp);
    yield(); 
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
  for (int i = 0; i < 4; i++) {
    printf("[DEBUG] PCB[%d] at %p (end at %x), stack range: [%p, %x]\n", 
           i, 
           &pcb[i], 
           (uintptr_t)&pcb[i] + sizeof(PCB),
           pcb[i].stack, 
           (uintptr_t)pcb[i].stack + sizeof(pcb[i].stack));
  }
    current = &pcb_boot;
    current->pid = 0;
    strcpy(current->name, "idle");
    PCB *p1 = pcb_alloc(); 
    char * argv[] = {NULL};
    char * envp[] = {NULL};
    context_uload(p1, "/bin/nterm", argv, envp);
    p1->state = READY;
    pcb_enqueue(p1);
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
    printf("Switching to PCB %s, cp at %p, pdir value at %p is %p\n", 
       next->name, next->cp, &(next->cp->pdir), next->cp->pdir);
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
