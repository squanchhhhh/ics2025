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

void init_pcb_meta(PCB *p, const char *name) {
  printf("proc %s init fd\n",name);
  strncpy(p->name, name, sizeof(p->name) - 1);
  // 1. 必须将所有 FD 初始化为 -1 (未分配)
  for (int i = 0; i < MAX_NR_PROC_FILE; i++) {
    p->fd_table[i] = -1;
  }
  // 2. 预留标准 IO
  p->fd_table[0] = 0; // stdin
  p->fd_table[1] = 1; // stdout
  p->fd_table[2] = 2; // stderr
}

void context_kload(PCB *pcb, void (*entry)(void *), void *arg) {
  init_pcb_meta(pcb,"kthread");
  Area kstack = RANGE(pcb->stack, pcb->stack + sizeof(pcb->stack));
  Context *cp = kcontext(kstack, entry, arg);
  pcb->cp = cp;
  printf("Kernel Thread Context at %p, its SP is %p\n", pcb->cp, (void *)pcb->cp->gpr[2]);
}
extern AddrSpace kas;
void context_uload(PCB *pcb, const char *filename, char *const argv[], char *const envp[]) {
  printf("load user proc %s\n",filename);
  init_pcb_meta(pcb,filename);
  // 1. 设置用户页表，protect 会将内核映射拷贝到用户页表
  protect(&pcb->as);
  // --- 插入这段调试代码 ---
  uint32_t *pdir = (uint32_t *)pcb->as.ptr;
  // 索引 512 对应虚拟地址 0x80000000 (0x80000000 >> 22 = 512)
  // 索引 1023 对应虚拟地址空间的最顶端
  printf("Checking user page table (pdir) at %p:\n", pdir);
  printf("  pdir[512] (0x80000000): 0x%08x\n", pdir[512]);
  printf("  pdir[1023] (0xffc00000): 0x%08x\n", pdir[1023]);
  // 2. 设置用户栈（32KB），并映射到页表
  uintptr_t v_top = (uintptr_t)pcb->as.area.end; // 通常是 0x80000000
  uintptr_t v_stack_low = v_top - 32 * 1024;
  void *pa_stack_top_page = NULL; 

  for (uintptr_t va = v_stack_low; va < v_top; va += PGSIZE) {
    void *pa = new_page(1);
    map(&pcb->as, (void *)va, pa, 7); // 权限: U, R, W
    if (va == v_top - PGSIZE) pa_stack_top_page = pa; 
  }

  // 3. 极简参数传递：仅设置 argc = 0
  // 我们在物理页的最顶端预留 16 字节空间，并确保对齐
  uintptr_t *sp_phys = (uintptr_t *)((uintptr_t)pa_stack_top_page + PGSIZE - 16);
  uintptr_t sp_virt = v_top - 16;
  
  *sp_phys = 0; // argc = 0

  // 4. 加载 ELF
  uintptr_t entry = loader(pcb, filename);

  // 5. 创建上下文
  Area kstack = RANGE(pcb->stack, pcb->stack + sizeof(pcb->stack));
  pcb->cp = ucontext(&pcb->as, kstack, (void *)entry);

  // 设置寄存器：a0 (gpr[10]) 是 argc, sp (gpr[2]) 是栈指针
  pcb->cp->gpr[10] = sp_virt;; 
  pcb->cp->gpr[2] = sp_virt;

  printf("Loader: entry = %x, SP = %x\n", entry, pcb->cp->gpr[2]);
}