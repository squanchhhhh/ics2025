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
  //printf("call set_satp, pdir = %x before resolve\n",pdir);
  uintptr_t mode = 1ul << (__riscv_xlen - 1);
  asm volatile("csrw satp, %0" : : "r"(mode | ((uintptr_t)pdir >> 12)));
  asm volatile("sfence.vma zero, zero"); 
}

static inline uintptr_t get_satp() {
  uintptr_t satp;
  asm volatile("csrr %0, satp" : "=r"(satp));
  if (satp == 0) return 0;
  return (satp & 0x3fffff) << 12; 
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
  if (c == NULL) return;
  
  // 1. 获取顶级页表物理地址
  void *pdir_addr = (c->pdir != NULL) ? c->pdir : kas.ptr;

  // 2. 调用原有的 set_satp (确保当前内核环境能正常访存)
  set_satp(pdir_addr);

  // 3. 【核心修正】将 pdir 字段修改为硬件 satp 格式
  // 这样接下来的 trap.S 在恢复上下文时，lw t0 加载到的就是 0x8008043c
  uintptr_t mode = 1ul << 31;
  uintptr_t ppn = (uintptr_t)pdir_addr >> 12;
  c->pdir = (void *)(mode | ppn); 

  // 4. 同步 TLB
  asm volatile("sfence.vma"); 
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

  // 1. 检查并建立一级页表项 (PDE)
  if (!(pgdir[vpn1] & 0x1)) { 
    void *new_pt = pgalloc_usr(PGSIZE);
    memset(new_pt, 0, PGSIZE); 
    
    uintptr_t pde_val = (((uintptr_t)new_pt >> 12) << 10) | 0x1;
    pgdir[vpn1] = pde_val;
    
    uintptr_t pt_addr = (pde_val >> 10) << 12;
    // 每开启一个新的 4MB 区域时打印一次，这很有参考价值
    printf("[VME] 新区域: VA [%p] 开启, 使用二级表 @ PA [%p]\n", 
           (void*)((uintptr_t)va & 0xffc00000), (void*)pt_addr);
  }

  // 2. 找到二级页表基地址
  uintptr_t *pgtab = (uintptr_t *)((pgdir[vpn1] >> 10) << 12);
  
  // 3. 填写二级页表项 (PTE)
  uintptr_t pte_val = (((uintptr_t)pa >> 12) << 10) | 0x1f;
  pgtab[vpn0] = pte_val;

  // 4. 精简打印：每 1024 页 (4MB) 抽查一次，或者在关键地址处抽查
  uintptr_t vpn = (uintptr_t)va >> 12;
  uintptr_t target_pa = (pte_val >> 10) << 12;
  
  if (vpn % 1024 == 0) {
    const char *tag = ((uintptr_t)va == target_pa) ? "[Identity OK]" : "[Mapping DIFF]";
    printf("[VME] PTE 抽查: VA %p -> PA %p %s\n", va, (void*)target_pa, tag);
  }
}

Context* ucontext(AddrSpace *as, Area kstack, void *entry) {
  Context *c = (Context *)((uintptr_t)kstack.end - sizeof(Context));
  memset(c, 0, sizeof(Context));
  c->mepc = (uintptr_t)entry;
  c->mstatus = 0x180 | 0x80;
  c->pdir = (as != NULL ? as->ptr : NULL);
  printf("set kernel stack for user, address = %p\n",c);
  return c;
}