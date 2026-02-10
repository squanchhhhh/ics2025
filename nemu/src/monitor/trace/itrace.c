#include "trace/itrace.h"
#include "trace/tui.h"
IRing iringbuf;

void push_inst(vaddr_t pc, const char *s) {
  iringbuf.pc[iringbuf.tail] = pc; 
  strncpy(iringbuf.buf[iringbuf.tail], s, LOG_BUF_LEN - 1);
  iringbuf.buf[iringbuf.tail][LOG_BUF_LEN - 1] = '\0';

  iringbuf.tail = (iringbuf.tail + 1) % RING_SIZE;
  if (iringbuf.num < RING_SIZE) iringbuf.num++;
}

void print_recent_insts() {
  printf("------- [ Recent Instructions (itrace) ] -------\n");
  int cnt = iringbuf.num;
  
  char filename[256];
  int line_num;

  for (int i = 0; i < cnt; i++) {
    get_pc_source(iringbuf.pc[i], filename, &line_num);

    if (i == iringbuf.error_idx) {
      printf("--> ");
    } else {
      printf("    ");
    }
    if (line_num != -1) {
      printf("%-50s | %s:%d\n", iringbuf.buf[i], filename, line_num);
    } else {
      printf("%-50s | unknown\n", iringbuf.buf[i]);
    }
  }
  printf("------------------------------------------------\n");
}

void init_iring(){
  iringbuf.tail = 0;
  iringbuf.num = 0;
  iringbuf.error_idx = -1;
}
