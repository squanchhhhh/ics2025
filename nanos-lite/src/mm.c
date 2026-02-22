#include <memory.h>
#include "list.h"
#include "proc.h"
#include "mm.h"
static LIST_HEAD(free_page_list);
static void *pf = NULL; 

/**
 * new_page - 分配连续的物理页帧
 * @nr_page: 需要分配的页数
 * 逻辑: 1. 优先从 free_page_list 中获取；2. 如果为空，则移动 pf 指针分配新页
 */
void* new_page(size_t nr_page) {
  if (nr_page == 1 && !list_empty(&free_page_list)) {
    list_head *node = free_page_list.next;
    list_del(node);
    return (void *)node;
  }
  void *p = pf;
  pf = (uint8_t *)pf + nr_page * PGSIZE;
  if (pf > heap.end) {
    panic("Out of physical memory!");
  }
  return p;
}

/**
 * free_page - 释放物理页帧
 * @p: 要释放的物理起始地址
 * 逻辑: 将页面挂入空闲链表，以便下次重用
 */
void free_page(void *p) {
  if (p == NULL) return;
  list_head *node = (list_head *)p;
  list_add_tail(node, &free_page_list);
}


#ifdef HAS_VME
/**
 * pg_alloc - 为页表分配物理内存并清零
 * @n: 需要分配的字节数
 * 返回值: 已清零的物理页地址
 * 逻辑: 被 vme_init 调用，作为 map() 过程中创建新页表的底层支撑
 */
static void* pg_alloc(int n) {
  size_t alloc_size = ROUNDUP(n, PGSIZE);
  void *ret = new_page(alloc_size / PGSIZE); 
  memset(ret, 0, alloc_size);
  return ret;
}
#endif

/**
 * mm_brk - 处理进程堆区扩展 (SYS_brk)
 * @brk: 进程请求的新堆顶地址
 * 返回值: 0 表示成功，非 0 表示失败
 * 逻辑: 检查地址合法性，并在必要时调用 map() 建立物理页映射
 */
int mm_brk(uintptr_t brk) {
  if (brk <= current->max_brk) {
    return 0;
  }
  uintptr_t va_start = ROUNDUP(current->max_brk, PGSIZE);
  uintptr_t va_end   = ROUNDUP(brk, PGSIZE);
  for (uintptr_t va = va_start; va < va_end; va += PGSIZE) {
    void *pa = new_page(1); 
    map(&current->as, (void *)va, pa, 7); 
  }
  current->max_brk = brk;
  return 0;
}

/**
 * init_mm - 初始化内存管理子系统
 * 逻辑: 1. 计算堆区起始位置；2. 预留内核自用内存 (1MB)；3. 开启 VME 接口
 */
void init_mm() {
  uintptr_t base = ROUNDUP(heap.start, PGSIZE);
  // 预留 1MB 物理内存给内核的 malloc (klib) 使用，防止与 VME 物理池冲突
  pf = (void *)(base + 0x100000); 
  Log("Free physical pages for VME starting from %p", pf);
#ifdef HAS_VME
  vme_init(pg_alloc, free_page);
#endif
}

// 辅助函数：查询页表以获取虚拟地址对应的物理地址 (手动解析 Sv32)
static void* pg_query(AddrSpace *as, void *va) {
  uintptr_t vpn1 = ((uintptr_t)va >> 22) & 0x3ff;
  uintptr_t vpn0 = ((uintptr_t)va >> 12) & 0x3ff;
  uintptr_t *pgdir = (uintptr_t *)as->ptr;

  if (!(pgdir[vpn1] & 0x1)) return NULL; // 一级页表项无效

  uintptr_t *pgtab = (uintptr_t *)((pgdir[vpn1] >> 10) << 12);
  if (!(pgtab[vpn0] & 0x1)) return NULL; // 二级页表项无效

  // 返回物理页起始地址
  return (void *)((pgtab[vpn0] >> 10) << 12);
}

#define MM_LOG 1

void copy_as(AddrSpace *dst, AddrSpace *src) {
  MLOG(MM_LOG, "copy_as: Cloning AS from PID %d to new child", current->pid);
  
  protect(dst);
  MLOG(MM_LOG, "copy_as: New child pgdir created at %p", dst->ptr);

  uintptr_t va_start = (uintptr_t)src->area.start;
  uintptr_t va_end   = (uintptr_t)current->max_brk;
  
  MLOG(MM_LOG, "copy_as: Scanning VA range [%p, %p]", (void *)va_start, (void *)va_end);
  int page_count = 0;
  for (uintptr_t va = va_start; va < va_end; va += PGSIZE) {
    void *pa_src = pg_query(src, (void *)va);
  
    if (pa_src != NULL) {
      void *pa_dst = new_page(1);
      if (!pa_dst) panic("copy_as: Out of memory during cloning!");
      
      memcpy(pa_dst, pa_src, PGSIZE);
      
      map(dst, (void *)va, pa_dst, 7); 
      
      page_count++;
      if (page_count % 64 == 0 || va >= (va_end - PGSIZE)) {
         MLOG(MM_LOG, "  [Page %d] Map VA %p: Parent PA %p -> Child PA %p", 
              page_count, (void *)va, pa_src, pa_dst);
      }
    }
  }
  uintptr_t stack_top = (uintptr_t)src->area.end;
  for (uintptr_t va = stack_top - 8 * PGSIZE; va < stack_top; va += PGSIZE) {
    void *pa_src = pg_query(src, (void *)va);
    if (pa_src != NULL) {
      void *pa_dst = new_page(1);
      memcpy(pa_dst, pa_src, PGSIZE);
      map(dst, (void *)va, pa_dst, 7);
      MLOG(MM_LOG, "  [Stack] Map VA %p: Parent PA %p -> Child PA %p", 
           (void *)va, pa_src, pa_dst);
    }
  }

  MLOG(MM_LOG, "copy_as: Finished cloning %d data pages + stack pages", page_count);
}