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
      // 计算虚拟地址范围
      uintptr_t vaddr_start = phdr.p_vaddr;
      uintptr_t vaddr_end   = phdr.p_vaddr + phdr.p_memsz;
      
      // 按页对齐范围
      uintptr_t pg_start = ROUNDDOWN(vaddr_start, PGSIZE);
      uintptr_t pg_end   = ROUNDUP(vaddr_end, PGSIZE);

      // 遍历每一个虚拟页
      for (uintptr_t va = pg_start; va < pg_end; va += PGSIZE) {
        // 1. 分配物理页并映射
        void *pa = new_page(1); 
        map(&pcb->as, (void *)va, pa, 0);
        
        // 2. 默认先清零整个物理页（处理 BSS 部分以及页内碎片）
        memset(pa, 0, PGSIZE);

        // 3. 计算这一页需要从文件中读入的内容
        // 计算当前页 va 对应的文件内容区间：[page_file_start, page_file_end)
        uintptr_t page_va_start = va;
        uintptr_t page_va_end   = va + PGSIZE;

        // 段在文件中的实际起止虚拟地址 (filesz 部分)
        uintptr_t file_vaddr_limit = vaddr_start + phdr.p_filesz;

        // 只有当当前页与段的文件有效数据区有交集时才读取
        if (page_va_end > vaddr_start && page_va_start < file_vaddr_limit) {
          
          // 计算在当前物理页内的偏移 (dst_off)
          uintptr_t dst_off = (page_va_start < vaddr_start) ? (vaddr_start - page_va_start) : 0;
          
          // 计算从文件的哪里开始读 (src_file_off)
          uintptr_t src_file_off = phdr.p_offset + ((page_va_start > vaddr_start) ? (page_va_start - vaddr_start) : 0);
          
          // 计算读多少 (len)
          uintptr_t read_end = (page_va_end < file_vaddr_limit) ? page_va_end : file_vaddr_limit;
          uintptr_t read_start = (page_va_start > vaddr_start) ? page_va_start : vaddr_start;
          uintptr_t len = read_end - read_start;

          // 执行读取
          vfs_lseek(fd, src_file_off, SEEK_SET);
          vfs_read(fd, (uint8_t *)pa + dst_off, len);
        }
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
  //1.设置用户页表，copy内核页表到用户页表
  protect(&pcb->as);
  //2.设置用户栈，并加入到页表中
  uintptr_t v_top = (uintptr_t)pcb->as.area.end;
  uintptr_t v_stack_low = v_top - 32 * 1024;
  void *pa_stack_top_page = NULL;  // 暂时保留一个物理地址用于传参
  for (uintptr_t va = v_stack_low; va < v_top; va += PGSIZE) {
    void *pa = new_page(1);
    map(&pcb->as, (void *)va, pa, 7); 
    if (va == v_top - PGSIZE) pa_stack_top_page = pa; 
  }

  // 3. 参数传递 
  int argc = 0;
  if (argv) { while (argv[argc]) argc++; }
  char *pa_ptr = (char *)pa_stack_top_page + PGSIZE; 
  uintptr_t v_ptr = v_top;
  uintptr_t argv_va[argc];
  for (int i = 0; i < argc; i++) {
    size_t len = strlen(argv[i]) + 1;
    pa_ptr -= len;
    v_ptr -= len;
    memcpy(pa_ptr, argv[i], len);
    argv_va[i] = v_ptr;
  }
  uintptr_t align = (uintptr_t)pa_ptr & 0x3;
  pa_ptr -= align;
  v_ptr -= align;
  uintptr_t *pa_argv_list = (uintptr_t *)pa_ptr;
  pa_argv_list--; 
  v_ptr -= sizeof(uintptr_t);
  for (int i = argc - 1; i >= 0; i--) {
    pa_argv_list--;
    v_ptr -= sizeof(uintptr_t);
    *pa_argv_list = argv_va[i];
  }
  pa_argv_list--;
  v_ptr -= sizeof(uintptr_t);
  *(int *)pa_argv_list = argc;

  // 4. 加载 ELF
  uintptr_t entry = loader(pcb, filename);
  printf("entry = %x\n",entry);
  // 5. 创建上下文
  Area kstack = RANGE(pcb->stack, pcb->stack + sizeof(pcb->stack));
  pcb->cp = ucontext(&pcb->as, kstack, (void *)entry);
  printf("user proc pdir = %x\n",v_ptr);
  pcb->cp->gpr[10] = argc;
  pcb->cp->gpr[2] = v_ptr;
  printf("Context pointer: %p, stored SP: %x\n", pcb->cp, pcb->cp->gpr[2]);
}