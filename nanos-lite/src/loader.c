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
  size_t len = strlen(str) + 1;
  *cur_sp -= len; 
  strcpy((char *)*cur_sp, str);
  return (char *)*cur_sp;
}

/*功能：加载用户程序
1. 加载 ELF 得到入口地址
2. 分配用户栈
3. 计算 argc, envp
4. 压栈
5. 准备内核栈上的上下文
*/
void context_uload(PCB *pcb, const char *filename, char *const argv[], char *const envp[]) {
  uintptr_t entry = loader(pcb, filename);

  void *ustack_bottom = new_page(8); 
  uintptr_t ustack_top = (uintptr_t)ustack_bottom + 8 * 4096;
  uintptr_t cur_sp = ustack_top;


  int argc = 0;
  if (argv) { while (argv[argc]) argc++; }
  int envc = 0;
  if (envp) { while (envp[envc]) envc++; }

 
  char *envp_ptr[envc];
  for (int i = 0; i < envc; i++) {
    envp_ptr[i] = stack_push_str(&cur_sp, envp[i]);
  }

  char *argv_ptr[argc];
  for (int i = 0; i < argc; i++) {
    argv_ptr[i] = stack_push_str(&cur_sp, argv[i]);
  }

  cur_sp &= ~0xf;

  uintptr_t *sp = (uintptr_t *)cur_sp;
  
  *(--sp) = 0; 
  for (int i = envc - 1; i >= 0; i--) {
    *(--sp) = (uintptr_t)envp_ptr[i];
  }

  *(--sp) = 0;
  for (int i = argc - 1; i >= 0; i--) {
    *(--sp) = (uintptr_t)argv_ptr[i];
  }

  *(--sp) = argc;

  Area kstack = { .start = pcb->stack, .end = pcb->stack + sizeof(pcb->stack) };
  pcb->cp = ucontext(NULL, kstack, (void *)entry);
  pcb->cp->GPRx = (uintptr_t)sp; 
  
  strcpy(pcb->name, filename);
}
