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
  printf("current fd = %d\n",fd);
  fs_close(fd); 
  return ehdr.e_entry;
}

void naive_uload(PCB *pcb, const char *filename) {
    uintptr_t entry = loader(pcb, filename);
    uintptr_t stack_top = (uintptr_t)heap.end & ~0xf; 
    
    // 在栈上压入 3 个 0 (argc, argv, envp) 以满足 C 库初始化
    uintptr_t *sp = (uintptr_t *)stack_top;
    sp -= 4;
    sp[0] = 0; 
    
    Log("Jump to entry = %p, SP = %p", (void *)entry, sp);
    asm volatile (
        "mv sp, %0; jr %1" 
        : : "r"(sp), "r"(entry) : "memory"
    );
}