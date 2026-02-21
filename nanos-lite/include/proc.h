#ifndef __PROC_H__
#define __PROC_H__

#include <common.h>
#include <memory.h>
void do_execve(const char *filename, char *const argv[], char *const envp[]) ;
#define STACK_SIZE (8 * PGSIZE)
#define MAX_NR_PROC_FILE 32

typedef struct {\
  Context *cp;
  AddrSpace as;
  uintptr_t max_brk;
  int fd_table[MAX_NR_PROC_FILE];                 
  char name[64];
  uint8_t stack[STACK_SIZE] PG_ALIGN;
} PCB;


extern PCB *current;
void naive_uload(PCB *pcb, const char *filename);
int map_to_proc_fd(int s_idx);
Context* schedule(Context *prev);
void context_uload(PCB *pcb, const char *filename, char *const argv[], char *const envp[])  ;
void context_kload(PCB *pcb, void (*entry)(void *), void *arg);
#endif
