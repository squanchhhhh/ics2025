#include <memory.h>

static void *pf = NULL;

void* new_page(size_t nr_page) {
  //printf("call new_page current pf = %p\n",pf);
  void *p = pf;
  pf = (uint8_t *)pf + nr_page * PGSIZE;
  return p;
}

#ifdef HAS_VME
static void* pg_alloc(int n) {
  size_t alloc_size = ROUNDUP(n, PGSIZE);
  void *ret = pf;
  
  pf = (uint8_t *)pf + alloc_size;

  memset(ret, 0, alloc_size);
  return ret;
}
#endif

void free_page(void *p) {
  panic("not implement yet");
}

/* The brk() system call handler. */
int mm_brk(uintptr_t brk) {
  return 0;
}

void init_mm() {
  pf = (void *)ROUNDUP(heap.start, PGSIZE);
  Log("free physical pages starting from %p", pf);

#ifdef HAS_VME
  vme_init(pg_alloc, free_page);
#endif
}