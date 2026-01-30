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
  int fd = fs_open(filename, 0, 0);
  if (fd < 0) panic("Open %s failed", filename);

  fs_read(fd, &ehdr, sizeof(Elf32_Ehdr));
  assert(*(uint32_t *)ehdr.e_ident == 0x464c457f);

  for (int i = 0; i < ehdr.e_phnum; i++) {
    Elf32_Phdr ph;
    fs_lseek(fd, ehdr.e_phoff + i * ehdr.e_phentsize, SEEK_SET);
    fs_read(fd, &ph, sizeof(Elf32_Phdr));

    if (ph.p_type == PT_LOAD) {
      fs_lseek(fd, ph.p_offset, SEEK_SET);
      fs_read(fd, (void *)ph.p_vaddr, ph.p_filesz);
      if (ph.p_memsz > ph.p_filesz) {
        memset((void *)(ph.p_vaddr + ph.p_filesz), 0, ph.p_memsz - ph.p_filesz);
      }
    }
  }
  
  //fs_close(fd); 
  return ehdr.e_entry;
}

void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, filename);
  Log("Jump to entry = %p", (void *)entry);
  ((void(*)())entry) ();
}

