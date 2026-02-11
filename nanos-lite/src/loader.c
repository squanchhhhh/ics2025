#include <proc.h>
#include <elf.h>
#include <fs.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef __LP64__
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
#else
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
#endif

uintptr_t loader(PCB *pcb, const char *filename) {
  Elf32_Ehdr ehdr;
  int fd = vfs_open(filename, 0);
  if (fd < 0) panic("Open %s failed", filename);
  vfs_read(fd, &ehdr, sizeof(Elf32_Ehdr));
  assert(*(uint32_t *)ehdr.e_ident == 0x464c457f);
  for (int i = 0; i < ehdr.e_phnum; i++) {
    Elf32_Phdr ph;
    vfs_lseek(fd, ehdr.e_phoff + i * ehdr.e_phentsize, SEEK_SET);
    vfs_read(fd, &ph, sizeof(Elf32_Phdr));
    if (ph.p_type == PT_LOAD) {
      vfs_lseek(fd, ph.p_offset, SEEK_SET);
      vfs_read(fd, (void *)ph.p_vaddr, ph.p_filesz);
      if (ph.p_memsz > ph.p_filesz) {
        memset((void *)(ph.p_vaddr + ph.p_filesz), 0, ph.p_memsz - ph.p_filesz);
      }
    }
  }
  //printf("current fd = %d\n",fd);
  vfs_close(fd); 
  return ehdr.e_entry;
}
/*
void naive_uload(PCB *pcb, const char *filename) {
    uintptr_t entry = loader(pcb, filename);
    uintptr_t stack_top = (uintptr_t)heap.end & ~0xf; 
    
    // 在栈上压入 3 个 0 (argc, argv, envp)
    uintptr_t *sp = (uintptr_t *)stack_top;
    sp -= 4;
    sp[0] = 0; 
    
    Log("Jump to entry = %p, SP = %p", (void *)entry, sp);
    asm volatile (
        "mv sp, %0; jr %1" 
        : : "r"(sp), "r"(entry) : "memory"
    );
}*/
void context_kload(PCB *pcb, void (*entry)(void *), void *arg) {
  Area kstack = { .start = pcb->stack, .end = pcb->stack + sizeof(pcb->stack) };
  pcb->cp = kcontext(kstack, entry, arg);
}

static char* stack_push_str(uintptr_t *cur_sp, const char *str) {
  if (!str) return NULL;
  size_t len = strlen(str) + 1;
  *cur_sp -= len;
  memcpy((void *)*cur_sp, str, len);
  return (char *)*cur_sp;
}

static uintptr_t setup_stack(uintptr_t sp_top, char *const argv[], char *const envp[]) {
  uintptr_t cur_sp = sp_top;
  int argc = 0, envc = 0;

  if (argv) { while (argv[argc]) argc++; }
  if (envp) { while (envp[envc]) envc++; }

  // 1. 先把字符串内容压入栈顶（从高地址往低地址）
  char *argv_va[argc];
  char *envp_va[envc];
  for (int i = envc - 1; i >= 0; i--) envp_va[i] = stack_push_str(&cur_sp, envp[i]);
  for (int i = argc - 1; i >= 0; i--) argv_va[i] = stack_push_str(&cur_sp, argv[i]);

  // 2. 预留指针数组的空间
  // 布局: [argc] [argv[0...n]] [NULL] [envp[0...n]] [NULL]
  // 总共需要的 4 字节(uintptr_t) 单元数量:
  int total_slots = 1 + argc + 1 + envc + 1;
  uintptr_t array_start = cur_sp - (total_slots * sizeof(uintptr_t));
  
  // 3. 16 字节对齐
  uintptr_t final_sp = array_start & ~0xf;
  
  // 4. 使用数组下标填充，绝对不会错位
  uintptr_t *ptr_stack = (uintptr_t *)final_sp;
  int k = 0;
  
  ptr_stack[k++] = (uintptr_t)argc;  // sp[0] = argc
  for (int i = 0; i < argc; i++) {
    ptr_stack[k++] = (uintptr_t)argv_va[i];
  }
  ptr_stack[k++] = 0;                // argv[argc] = NULL
  
  for (int i = 0; i < envc; i++) {
    ptr_stack[k++] = (uintptr_t)envp_va[i];
  }
  ptr_stack[k++] = 0;                // envp[envc] = NULL

  // 5. 最后的验证 Log
  Log("Stack verification: sp=%p, argc=%d, argv[0] at %p is %s", 
      (void*)final_sp, (int)ptr_stack[0], (void*)ptr_stack[1], (char*)ptr_stack[1]);

  return final_sp;
}

void context_uload(PCB *pcb, const char *filename, char *const argv[], char *const envp[]) {
  // 1. 加载 ELF
  uintptr_t entry = loader(pcb, filename);
  
  // 2. 准备用户栈
  void *ustack_bottom = new_page(8); 
  uintptr_t ustack_top = (uintptr_t)ustack_bottom + 8 * 4096;

  // 3. 调用拆分后的压栈函数
  uintptr_t final_sp = setup_stack(ustack_top, argv, envp);

  // 4. 准备上下文和进程信息
  Area kstack = { .start = pcb->stack, .end = pcb->stack + sizeof(pcb->stack) };
  
  for (int i = 0; i < MAX_NR_PROC_FILE; i++) pcb->fd_table[i] = -1; 
  pcb->fd_table[0] = 0; pcb->fd_table[1] = 1; pcb->fd_table[2] = 2;

  pcb->cp = ucontext(NULL, kstack, (void *)entry);
  pcb->cp->GPRx = final_sp; // RISC-V: a0 = sp

  strcpy(pcb->name, filename);
  Log("DEBUG BEFORE RETURN: sp=%p, argc=%d, argv[0]=%s", (void*)final_sp, *(int*)final_sp, ((char**)final_sp)[1]);
}