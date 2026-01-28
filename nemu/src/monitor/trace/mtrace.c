#include "trace/mtrace.h"

void push_mtrace(MTraceBuffer * buf,vaddr_t pc,paddr_t addr,uint64_t data,int len,MemAccessType type){
  //排除取指
  if(addr == pc){
    return ;
  }
    buf->buf[buf->tail].addr = addr;
    buf->buf[buf->tail].data = data;
    buf->buf[buf->tail].len = len;
    buf->buf[buf->tail].pc = pc;
    buf->buf[buf->tail].type = type;
    buf->tail = (buf->tail+1)%MTRACE_BUF_SIZE;
    if (buf->num<MTRACE_BUF_SIZE){
    buf->num++;}
}
MTraceBuffer mtrace_buf;
bool mtrace_inited = false;
void init_mtrace(void) {
    if (!mtrace_inited) {
        mtrace_buf.num = 0;
        mtrace_buf.tail = 0;
        mtrace_inited = true;
    }
}

void dump_mtrace(void) {
    if (!mtrace_inited || mtrace_buf.num == 0) {
        Log("No memory trace records.");
        return;
    }
    Log("Recent memory trace (%d entries):", mtrace_buf.num);
    int start = (mtrace_buf.tail - mtrace_buf.num + MTRACE_BUF_SIZE)
                % MTRACE_BUF_SIZE;
    for (int i = 0; i < mtrace_buf.num; i++) {
        int idx = (start + i) % MTRACE_BUF_SIZE;
        MTraceEntry *e = &mtrace_buf.buf[idx];
        printf("%s pc=%x addr=%x len=%dbyte data(hex)=0x%08lx data(dec)=%d\n", (e->type == MEM_READ) ? "R" : "W",e->pc,e->addr,e->len,(uint64_t)e->data,(int)e->data);
    }
}