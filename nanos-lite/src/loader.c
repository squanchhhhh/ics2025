#include "common.h"
#include "memory.h"
#include <elf.h>
#include <fs.h>
#include <proc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef __LP64__
#define Elf_Ehdr Elf64_Ehdr
#define Elf_Phdr Elf64_Phdr
#else
#define Elf_Ehdr Elf32_Ehdr
#define Elf_Phdr Elf32_Phdr
#endif

uintptr_t loader(PCB *pcb, const char *filename) {
  int fd = vfs_open(filename, 0);
  panic_on(fd < 0, "Open file failed in loader");
  Elf_Ehdr ehdr;
  vfs_read(fd, &ehdr, sizeof(Elf_Ehdr));
  panic_on(memcmp(ehdr.e_ident, ELFMAG, 4) != 0, "Invalid ELF file");
  Elf_Phdr phdr;
  for (int i = 0; i < ehdr.e_phnum; i++) {
    vfs_lseek(fd, ehdr.e_phoff + i * sizeof(Elf_Phdr), SEEK_SET);
    vfs_read(fd, &phdr, sizeof(Elf_Phdr));
    if (phdr.p_type == PT_LOAD) {
      uintptr_t vaddr_start = phdr.p_vaddr;
      uintptr_t vaddr_end   = phdr.p_vaddr + phdr.p_memsz;
      uintptr_t pg_start = ROUNDDOWN(vaddr_start, PGSIZE);
      uintptr_t pg_end   = ROUNDUP(vaddr_end, PGSIZE);
      uintptr_t file_offset = phdr.p_offset;
      uintptr_t file_left   = phdr.p_filesz;
      uintptr_t inner_off   = vaddr_start & (PGSIZE - 1);
      for (uintptr_t va = pg_start; va < pg_end; va += PGSIZE) {
        void *pa = new_page(1); 
        map(&pcb->as, (void *)va, pa, 0);
        memset(pa, 0, PGSIZE);
        if (file_left > 0) {
          uintptr_t read_len = (file_left < PGSIZE - inner_off) ? file_left : (PGSIZE - inner_off);
          vfs_lseek(fd, file_offset, SEEK_SET);
          vfs_read(fd, pa + inner_off, read_len);
          file_left   -= read_len;
          file_offset += read_len;
        }
        inner_off = 0;
      }
    }
  }
  vfs_close(fd);
  return ehdr.e_entry;
}

void context_kload(PCB *pcb, void (*entry)(void *), void *arg) {
  Area kstack = RANGE(pcb->stack, pcb->stack + sizeof(pcb->stack));
  Context *cp = kcontext(kstack, entry, arg);
  pcb->cp = cp;
}
void context_uload(PCB *pcb, const char *filename, char *const argv[], char *const envp[]) {
  // 1. 创建地址空间 (初始化 as->ptr)
  protect(&pcb->as);
  printf("ptr after protect: %p\n", pcb->as.ptr);

  // 2. 分配 32KB 用户栈并映射
  uintptr_t v_top = (uintptr_t)pcb->as.area.end;
  uintptr_t v_stack_low = v_top - 32 * 1024;
  void *pa_stack_top_page = NULL;

  for (uintptr_t va = v_stack_low; va < v_top; va += PGSIZE) {
    void *pa = new_page(1);
    map(&pcb->as, (void *)va, pa, 7); // 可读可写可执行
    if (va == v_top - PGSIZE) pa_stack_top_page = pa; 
  }

  // 3. 参数传递 (简易版 setup_stack)
  // 我们在 pa_stack_top_page 物理页内操作
  // 假设我们把字符串和指针数组都放在这最后一个页面内 (4KB 足够)
  
  int argc = 0;
  if (argv) { while (argv[argc]) argc++; }

  // 我们从物理页底部 (即 v_top 对应的物理位置) 开始倒着压
  char *pa_ptr = (char *)pa_stack_top_page + PGSIZE; 
  uintptr_t v_ptr = v_top;

  // 存放 argv 字符串在用户空间虚拟地址的数组
  uintptr_t argv_va[argc];

  // 压入字符串
  for (int i = 0; i < argc; i++) {
    size_t len = strlen(argv[i]) + 1;
    pa_ptr -= len;
    v_ptr -= len;
    memcpy(pa_ptr, argv[i], len);
    argv_va[i] = v_ptr;
  }

  // 对齐到 4 字节 (RISC-V 要求栈对齐)
  uintptr_t align = (uintptr_t)pa_ptr & 0x3;
  pa_ptr -= align;
  v_ptr -= align;

  // 压入 argv 指针数组 (以 NULL 结尾)
  uintptr_t *pa_argv_list = (uintptr_t *)pa_ptr;
  pa_argv_list--; // 留出 NULL 的位置
  v_ptr -= sizeof(uintptr_t);
  
  for (int i = argc - 1; i >= 0; i--) {
    pa_argv_list--;
    v_ptr -= sizeof(uintptr_t);
    *pa_argv_list = argv_va[i];
  }

  // 压入 argc
  pa_argv_list--;
  v_ptr -= sizeof(uintptr_t);
  *(int *)pa_argv_list = argc;

  // 4. 加载 ELF
  uintptr_t entry = loader(pcb, filename);

  // 5. 创建上下文
  Area kstack = RANGE(pcb->stack, pcb->stack + sizeof(pcb->stack));
  pcb->cp = ucontext(&pcb->as, kstack, (void *)entry);

  // 6. 将最终计算出的虚拟 SP 传给 a0
  pcb->cp->GPRx = v_ptr;
}