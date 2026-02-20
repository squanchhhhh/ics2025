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