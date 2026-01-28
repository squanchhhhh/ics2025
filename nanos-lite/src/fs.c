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
static Finfo file_table[] __attribute__((used)) = {
  [FD_STDIN]  = {"stdin", 0, 0, invalid_read, invalid_write},
  [FD_STDOUT] = {"stdout", 0, 0, invalid_read, invalid_write},
  [FD_STDERR] = {"stderr", 0, 0, invalid_read, invalid_write},
#include "files.h"
};

void init_fs() {
  // TODO: initialize the size of /dev/fb
}



void indirect_fs_read();
int logic_to_physic(VFS_Inode *inode, int logic_num) {
  if (logic_num < NDIRECT) {
    return inode->addrs[logic_num];
  } else {
    uint32_t indirect_block_num = inode->addrs[NDIRECT];  //addrs的最后一项是间接块
    uint32_t indirect_content[BSIZE / sizeof(uint32_t)];  //间接块的内容是32位的物理块地址
    ramdisk_read(indirect_content, indirect_block_num * BSIZE, BSIZE);  //读取间接块到表中
    return indirect_content[logic_num - NDIRECT]; //根据表来确定实际的物理地址
  }
}

size_t fs_read(int fd, char *buf, size_t len) {
  F_Desc *f = &fd_table[fd];
  VFS_Inode *inode = f->ptr_inode;
  if (f->open_offset >= inode->size) return 0;
  if (f->open_offset + len > inode->size) {  //如果长度大于剩余的文件大小，则读取整个文件
    len = inode->size - f->open_offset; 
  }
  size_t total_read = 0;
  while (total_read < len) {
    uint32_t logic_num = f->open_offset / BSIZE;
    uint32_t off = f->open_offset % BSIZE;
    int physic_num = logic_to_physic(inode, logic_num);
    size_t can_read = BSIZE - off;
    if (can_read > (len - total_read)) {
      can_read = len - total_read;
    }
    ramdisk_read(buf + total_read, physic_num * BSIZE + off, can_read);
    total_read += can_read;
    f->open_offset += can_read;
  }
  
  return total_read; 
}