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

void init_proc() {
  switch_boot_pcb();
  Log("Initializing processes...");
  char * file_name = "/bin/nterm";
  current->name = file_name;
  for (int i = 0; i < MAX_NR_PROC_FILE; i++) {
      if (i < 3) {
        current->fd_table[i] = alloc_system_fd(i, 0); 
      } else {
        current->fd_table[i] = -1;
      }
    }
  naive_uload(NULL, file_name);
}

Context* schedule(Context *prev) {
  return NULL;
}
int map_to_proc_fd(int s_idx) {
  for (int i = 3; i < MAX_NR_PROC_FILE; i++) {
    if (current->fd_table[i] == -1) { 
      current->fd_table[i] = s_idx;
      return i;
    }
  }
  printf("Process %s: No available FD slots!\n", current->name);
  return -1;
}

void do_execve(const char *filename) {
    naive_uload(NULL, filename);
}