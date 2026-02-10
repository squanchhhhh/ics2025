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
      printf("Loading PHDR: vaddr 0x%x, memsz 0x%x\n", ph.p_vaddr, ph.p_memsz);
      vfs_lseek(fd, ph.p_offset, SEEK_SET);
      vfs_read(fd, (void *)ph.p_vaddr, ph.p_filesz);
      if (ph.p_memsz > ph.p_filesz) {
        memset((void *)(ph.p_vaddr + ph.p_filesz), 0, ph.p_memsz - ph.p_filesz);
      }
    }
  }
  fs_close(fd); 
  return ehdr.e_entry;
}

void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, filename);

  extern Area heap;
  void *stack_top = heap.end; 

  Log("Jump to entry = %p, SP set to %p", (void *)entry, stack_top);
  printf("\n=== [DEBUG] Pre-Jump System Table Check ===\n");
  extern OpenFile system_open_table[]; // 确保能访问到它
  for (int i = 0; i < 8; i++) {
    printf("sys_idx [%d]: used=%d, fidx=%d, name=%s\n", 
           i, 
           system_open_table[i].used, 
           system_open_table[i].file_idx,
           system_open_table[i].used ? file_table[system_open_table[i].file_idx].name : "NONE");
  }
  printf("============================================\n\n");
  asm volatile (
    "mv sp, %0; jr %1" 
    : 
    : "r"(stack_top), "r"(entry) 
    : "memory"
  );
  panic("Should not reach here!");
}