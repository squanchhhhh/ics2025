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
  // 1. 加载 ELF 得到入口地址
  uintptr_t entry = loader(pcb, filename);
  Log("ELF entry at %p", (void*)entry);

  // 2. 分配用户栈 (32KB)
  void *ustack_bottom = new_page(8); 
  uintptr_t ustack_top = (uintptr_t)ustack_bottom + 8 * 4096;
  uintptr_t cur_sp = ustack_top;

  // 3. 计算 argc 和 envc
  int argc = 0;
  if (argv) { while (argv[argc]) argc++; }
  int envc = 0;
  if (envp) { while (envp[envc]) envc++; }

  // 4. 第一步：将所有字符串内容压入栈顶（从高地址往低地址）
  // 压入 envp 字符串内容
  char *envp_ptr[envc];
  for (int i = envc - 1; i >= 0; i--) {
    envp_ptr[i] = stack_push_str(&cur_sp, envp[i]);
  }

  // 压入 argv 字符串内容
  char *argv_ptr[argc];
  for (int i = argc - 1; i >= 0; i--) {
    argv_ptr[i] = stack_push_str(&cur_sp, argv[i]);
  }

  // 5. 第二步：在字符串区域下方，预留并填充指针数组
  uintptr_t *sp = (uintptr_t *)cur_sp;

  // 压入 envp 指针数组（以 NULL 结尾）
  sp--; *sp = 0; 
  for (int i = envc - 1; i >= 0; i--) {
    sp--; *sp = (uintptr_t)envp_ptr[i];
  }

  // 压入 argv 指针数组（以 NULL 结尾）
  sp--; *sp = 0; 
  for (int i = argc - 1; i >= 0; i--) {
    sp--; *sp = (uintptr_t)argv_ptr[i];
  }

  // 压入 argc
  sp--; *sp = (uintptr_t)argc;

  // 6. 第三步：强制 16 字节对齐
  // RISC-V 调用规范要求 sp 在进入入口点时必须对齐
  uintptr_t final_sp = (uintptr_t)sp & ~0xf;

  // 调试信息：确认 BusyBox 能看到什么
  Log("---- User Stack Layout Check for %s ----", filename);
  Log("Final SP set to: %p", (void*)final_sp);
  Log("argc = %d at %p", *(int *)final_sp, (void*)final_sp);
  char **check_argv = (char **)((uintptr_t)final_sp + sizeof(uintptr_t));
  Log("argv[0] = %p (\"%s\")", check_argv[0], check_argv[0]);

  // 7. 初始化 PCB 状态
  Area kstack = { .start = pcb->stack, .end = pcb->stack + sizeof(pcb->stack) };
  
  // 初始化文件描述符表，防止垃圾值
  for (int i = 0; i < MAX_NR_PROC_FILE; i++) {
    pcb->fd_table[i] = -1; 
  }
  pcb->fd_table[0] = 0; // stdin
  pcb->fd_table[1] = 1; // stdout
  pcb->fd_table[2] = 2; // stderr

  // 创建用户上下文
  pcb->cp = ucontext(NULL, kstack, (void *)entry);
  
  // 关键：将 a0 (GPRx) 设置为对齐后的栈指针
  pcb->cp->GPRx = final_sp; 

  // 设置进程名
  strcpy(pcb->name, filename);
}