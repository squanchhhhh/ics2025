#include "common.h"
#include "debug.h"
#include "proc.h"
#include <fs.h>
#include <stdlib.h>
#include <string.h>

enum {FD_STDIN, FD_STDOUT, FD_STDERR, FD_FB};

static struct superblock sb_copy;
size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

/* This is the information about all files in disk. */
Finfo file_table[MAX_STATIC_FILE] __attribute__((used)) = {
  [FD_STDIN]  = { .name = "stdin", .read = invalid_read, .write = invalid_write },
  [FD_STDOUT] = { .name = "stdout", .read = invalid_read, .write = invalid_write },
  [FD_STDERR] = { .name = "stderr", .read = invalid_read, .write = invalid_write },
  //#include "files.h" 
};
int nr_static_file = 3;
OpenFile system_open_table[MAX_OPEN_FILES];
int bread(char * buf,int offset){
  return ramdisk_read(buf,offset*BSIZE,BSIZE);
}
int bwrite(char * buf,int offset){
  return ramdisk_write(buf,offset*BSIZE,BSIZE);
}
// 返回物理块号
static uint32_t bmap(Finfo *f, int logical_block_idx) {
  if (logical_block_idx < NDIRECT) {
    return f->inode.addrs[logical_block_idx];
  }
  // TODO: 间接块
  panic("File too big! Indirect block not implemented.");
  return 0;
}

//在系统打开文件表中分配一个表项
int alloc_system_fd() {
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (!system_open_table[i].used) {
      system_open_table[i].used = true;
      system_open_table[i].open_offset = 0;
      return i;
    }
  }
  panic("System open file table overflow!");
  return -1;
}
//回收表项
void free_system_fd(int s_idx) {
  system_open_table[s_idx].used = false;
}

// 根据 inum 读取磁盘上的 dinode
void get_dinode(uint32_t inum, struct dinode *di) {
  uint32_t inode_table_start_byte = sb_copy.inode_start * BSIZE;
  uint32_t offset = inum * sizeof(struct dinode);
  ramdisk_read(di, inode_table_start_byte + offset, sizeof(struct dinode));
}

int fs_open(const char *filename, int flags, int mode) {
  int file_idx = -1;
  for (int i = 0; i < nr_static_file; i++) {
    if (file_table[i].name && strcmp(filename, file_table[i].name) == 0) {
      file_idx = i;
      break; 
    }
  }

  if (file_idx == -1) {
    Log("File not found: %s", filename);
    return -1; 
  }

  int sys_fd = alloc_system_fd();
  if (sys_fd == -1) return -1;

  system_open_table[sys_fd].file_idx = file_idx;
  system_open_table[sys_fd].open_offset = 0;
  system_open_table[sys_fd].used = true;
  return sys_fd;

 // int fd = add_fd_to_current_proc(sys_fd);
 // return fd; 
}
void fstate(int fd,struct dinode*d){
  int s_idx = current->fd_table[fd];
  OpenFile *of = &system_open_table[s_idx];
  Finfo *f = &file_table[of->file_idx];
  memcpy(d, &f->inode, sizeof(struct dinode));
}

size_t fs_read(int fd, void *buf, size_t len) {
  int s_idx = current->fd_table[fd];
  OpenFile *of = &system_open_table[s_idx];
  Finfo *f = &file_table[of->file_idx];

  // 特殊设备文件
  if (f->read != NULL) {
    return f->read(buf, 0, len); 
  }

  // 边界检查
  if (of->open_offset >= f->inode.size) return 0;
  if (of->open_offset + len > f->inode.size) {
    len = f->inode.size - of->open_offset;
  }

  size_t to_read = len;
  size_t current_off = of->open_offset; 

  while (to_read > 0) {
    uint32_t l_block = current_off / BSIZE;   // 逻辑块号
    uint32_t b_offset = current_off % BSIZE;  // 块内偏移
    uint32_t p_block = bmap(f, l_block);      // 获取物理块号

    size_t can_read = BSIZE - b_offset;
    size_t actual_read = (to_read < can_read) ? to_read : can_read;

    ramdisk_read((char *)buf + (len - to_read), p_block * BSIZE + b_offset, actual_read);

    current_off += actual_read;
    to_read -= actual_read;
  }

  of->open_offset = current_off;

  return len; 
}

size_t fs_lseek(int fd, size_t offset, int whence) {
  int s_idx = current->fd_table[fd];
  OpenFile *of = &system_open_table[s_idx];
  
  Finfo *f = &file_table[of->file_idx];

  size_t target_offset = of->open_offset;

  switch (whence) {
    case SEEK_SET:
      target_offset = offset;
      break;
    case SEEK_CUR:
      target_offset += offset;
      break;
    case SEEK_END:
      target_offset = f->inode.size + offset;
      break;
    default:
      return -1;
  }

  if (target_offset > f->inode.size) {
    target_offset = f->inode.size;
  }

  of->open_offset = target_offset;
  return target_offset;
}

void init_fs() {
  char buf[BSIZE];
  bread(buf, 1 ); 
  memcpy(&sb_copy, buf, sizeof(sb_copy));

  if (sb_copy.magic != 0x20010124) panic("Magic Mismatch!");

  struct dinode root;
  get_dinode(sb_copy.root_inum, &root);

  struct dirent *dir_entries = malloc(root.size);

  //暂时不考虑间接块
  for (int i = 0; i < (root.size + BSIZE - 1) / BSIZE; i++) {
    ramdisk_read((char *)dir_entries + i * BSIZE, root.addrs[i] * BSIZE, BSIZE);
  }

  int ndirs = root.size / sizeof(struct dirent);
  for (int i = 0; i < ndirs; i++) {
    if (dir_entries[i].inum == 0) continue; 

    Finfo *f = &file_table[nr_static_file];
    
    f->name = strdup(dir_entries[i].name); 
    
    get_dinode(dir_entries[i].inum, &f->inode);
    
    f->read = NULL;  
    f->write = NULL; 
    f->ref = 1;
    Log("load file %s, filesize: %d", f->name,f->inode.size);
    nr_static_file++;
  }
  free(dir_entries); 
  Log("FS initialized. %d files loaded.", nr_static_file);
}
