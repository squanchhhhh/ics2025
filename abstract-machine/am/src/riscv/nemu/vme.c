#include <am.h>
#include <nemu.h>
#include <klib.h>

static AddrSpace kas = {};
static void* (*pgalloc_usr)(int) = NULL;
static void (*pgfree_usr)(void*) = NULL;
static int vme_enable = 0;

static Area segments[] = {      // Kernel memory mappings
  NEMU_PADDR_SPACE
};

#define USER_SPACE RANGE(0x40000000, 0x80000000)

static inline void set_satp(void *pdir) {
  uintptr_t mode = 1ul << (__riscv_xlen - 1);
  asm volatile("csrw satp, %0" : : "r"(mode | ((uintptr_t)pdir >> 12)));
  asm volatile("sfence.vma zero, zero"); 
}

static inline uintptr_t get_satp() {
  uintptr_t satp;
  asm volatile("csrr %0, satp" : "=r"(satp));
  return satp << 12;
}

bool vme_init(void* (*pgalloc_f)(int), void (*pgfree_f)(void*)) {
  pgalloc_usr = pgalloc_f;
  pgfree_usr = pgfree_f;

  kas.ptr = pgalloc_f(PGSIZE);

  int i;
  for (i = 0; i < LENGTH(segments); i ++) {
    void *va = segments[i].start;
    for (; va < segments[i].end; va += PGSIZE) {
      map(&kas, va, va, 0);
    }
  }

  set_satp(kas.ptr);
  vme_enable = 1;

  return true;
}

void protect(AddrSpace *as) {
  PTE *updir = (PTE*)(pgalloc_usr(PGSIZE));
  as->ptr = updir;
  as->area = USER_SPACE;
  as->pgsize = PGSIZE;
  // map kernel space
  memcpy(updir, kas.ptr, PGSIZE);
}

void unprotect(AddrSpace *as) {
}

void __am_get_cur_as(Context *c) {
  c->pdir = (vme_enable ? (void *)get_satp() : NULL);
}

void __am_switch(Context *c) {
  if (vme_enable && c->pdir != NULL) {
    set_satp(c->pdir);
  }
}

/*
功能：在页表中填入页表项
 1. 提取虚拟地址的 VPN[1] (一级索引) 和 VPN[0] (二级索引)
 2. 找到一级页表基地址 (Page Directory)
 3. 检查一级页表项 (PDE)，将新页表的物理地址转换成 PDE 格式填入 (PPN 位在 10-31)
 4. 找到二级页表基地址 (Page Table)，从 PDE 中提取 PPN 部分并还原成物理地址
5. 填写二级页表项 (PTE)
*/
void map(AddrSpace *as, void *va, void *pa, int prot) {

  uintptr_t vpn1 = ((uintptr_t)va >> 22) & 0x3ff;
  uintptr_t vpn0 = ((uintptr_t)va >> 12) & 0x3ff;
  
  uintptr_t *pgdir = (uintptr_t *)as->ptr;

  if (!(pgdir[vpn1] & 0x1)) { 
    void *new_pt = pgalloc_usr(PGSIZE);
    pgdir[vpn1] = (((uintptr_t)new_pt >> 12) << 10) | 0x1; 
  }

  uintptr_t *pgtab = (uintptr_t *)((pgdir[vpn1] >> 10) << 12);

  pgtab[vpn0] = (((uintptr_t)pa >> 12) << 10) | 0xf;
}

Context* ucontext(AddrSpace *as, Area kstack, void *entry) {
  Context *c = (Context *)((uintptr_t)kstack.end - sizeof(Context));
  c->mepc = (uintptr_t)entry;
  c->mstatus = 0x1800; 
  c->pdir = (as != NULL ? as->ptr : NULL);
  return c;
}
