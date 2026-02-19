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
  protect(&pcb->as);
  uintptr_t v_top = (uintptr_t)pcb->as.area.end;
  uintptr_t v_bottom = v_top - 8 * PGSIZE;
  for (uintptr_t va = v_bottom; va < v_top; va += PGSIZE) {
    void *pa = new_page(1);
    map(&pcb->as, (void *)va, pa, 0);
  }
  uintptr_t entry = loader(pcb, filename);
  // 内核栈依然使用 pcb->stack
  Area kstack = RANGE(pcb->stack, pcb->stack + sizeof(pcb->stack));
  Context *cp = ucontext(&pcb->as, kstack, (void *)entry);
  // 设置初始用户栈指针 (Virtual SP)
  cp->GPRx = v_top; 
  pcb->cp = cp;
}