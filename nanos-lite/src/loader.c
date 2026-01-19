#include <proc.h>
#include <elf.h>

#ifdef __LP64__
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
#else
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
#endif

uintptr_t loader(PCB *pcb, const char *filename) {
  Elf32_Ehdr ehdr;
  ramdisk_read(&ehdr, 0, sizeof(Elf32_Ehdr));
  assert(*(uint32_t *)ehdr.e_ident == 0x464c457f); 
  for (int i = 0; i < ehdr.e_phnum; i++) {
    Elf32_Phdr ph;
    ramdisk_read(&ph, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(Elf32_Phdr));
    if (ph.p_type == PT_LOAD) {
      ramdisk_read((void *)ph.p_vaddr, ph.p_offset, ph.p_filesz);
      memset((void *)(ph.p_vaddr + ph.p_filesz), 0, ph.p_memsz - ph.p_filesz);
    }
  }
  return ehdr.e_entry;
}

void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, filename);
  Log("Jump to entry = %p", entry);
  ((void(*)())entry) ();
}

