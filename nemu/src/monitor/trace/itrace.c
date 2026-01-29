#include "trace/itrace.h"

IRing iringbuf;

void push_inst(const char * s){
  strncpy(iringbuf.buf[iringbuf.tail], s, LOG_BUF_LEN - 1);
  iringbuf.buf[iringbuf.tail][LOG_BUF_LEN - 1] = '\0';
  iringbuf.tail = (iringbuf.tail+1)%RING_SIZE;
  if (iringbuf.num < RING_SIZE){
    iringbuf.num++;
  }
  return;
}

void print_recent_insts() {
  int cnt = iringbuf.num;
  for (int i = 0;i<cnt;i++){
    if (i ==iringbuf.error_idx){
      log_write("-->%s\n", iringbuf.buf[i]);
    }else {
      log_write("   %s\n", iringbuf.buf[i]);
    }
  }
}

void init_iring(){
  iringbuf.tail = 0;
  iringbuf.num = 0;
}
