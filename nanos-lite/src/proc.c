#include "debug.h"
#include <proc.h>
#include <stdio.h>
#include <string.h>

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
  char * file_name = "/bin/serial";
  current->name = file_name;
  Log("load process name %s",current->name);
  current->nr_fd = 3;
  //初始化stdin stdout stderr
  current->fd_table[0] = 0; 
  current->fd_table[1] = 1;
  current->fd_table[2] = 2;
  naive_uload(NULL, file_name);
}

Context* schedule(Context *prev) {
  return NULL;
}
int map_to_proc_fd(int s_idx) {
  if (current->nr_fd >= MAX_NR_PROC_FILE) {
    printf("current proc %s cannot open more file\n",current->name);
    return -1; 
  }
  //printf("system open_file table idx = %d, and return to proc fd = %d\n",s_idx,current->nr_fd);
  current->fd_table[current->nr_fd] = s_idx;
  return current->nr_fd++;
}
