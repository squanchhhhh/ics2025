#include "common.h"
#include "debug.h"
#include <fs.h>
#include <string.h>

enum {FD_STDIN, FD_STDOUT, FD_STDERR, FD_FB};

size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

/* This is the information about all files in disk. */
Finfo file_table[] __attribute__((used)) = {
  [FD_STDIN]  = { .name = "stdin", .read = invalid_read, .write = invalid_write },
  [FD_STDOUT] = { .name = "stdout", .read = invalid_read, .write = invalid_write },
  [FD_STDERR] = { .name = "stderr", .read = invalid_read, .write = invalid_write },
  //#include "files.h" 
};

int bread(char * buf,int offset){
  return ramdisk_read(buf,offset*BSIZE,BSIZE);
}
int bwrite(char * buf,int offset){
  return ramdisk_write(buf,offset*BSIZE,BSIZE);
}
void init_fs() {
  char buf[BSIZE];
  bread(buf, 1);
  struct superblock sb;
  memcpy(&sb, buf, sizeof(sb));
  printf("%x\n",sb.magic);
  // TODO: initialize the size of /dev/fb
}



