/* * Nanos-lite Virtual File System (VFS) Module
 * * --- 底层磁盘与 Inode 辅助函数 ---
 * - bread(buf, offset): 以块(BSIZE)为单位读取物理磁盘。
 * - bwrite(buf, offset): 以块(BSIZE)为单位写入物理磁盘。
 * - bmap(f, logical_idx): 将文件的逻辑块号映射为物理块号（处理数据分布）。
 * - get_dinode(inum, di): 根据 Inode 编号从磁盘读取原始 Inode 结构。
 * * --- 系统打开文件表管理 ---
 * - alloc_system_fd(): 在全局表 system_open_table 中分配一个空项，返回索引。
 * - free_system_fd(s_idx): 释放全局表中的指定项。
 * * --- 核心文件系统接口 (对接 System Calls) ---
 * - fs_open(name, flags, mode): 查找文件并关联到系统打开文件表。
 * - fs_read(fd, buf, len): 根据 open_offset 从物理块或设备读取数据，并更新偏移。
 * - fs_write(fd, buf,len): 向设备或物理块写入数据。
 * - fs_lseek(fd, offset, whence): 修改文件的读写指针 open_offset。
 * - fstate(fd, d): 获取指定文件的 Inode 信息（状态）。
 * * --- 初始化 ---
 * - init_fs(): 读取超级块(SB)，解析根目录，将磁盘文件加载到 file_table。
 */
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
  if (fd < 0 || current->fd_table[fd] < 0) return -1;
  int s_idx = current->fd_table[fd];
  OpenFile *of = &system_open_table[s_idx];
  Finfo *f = &file_table[of->file_idx];

  // 特殊设备文件
  if (f->read != NULL) {
    size_t result = f->read(buf, of->open_offset, len);
    of->open_offset += result;
    return result; 
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

//ssize_t write(int fd, const void *buf, size_t count);
size_t fs_write(int fd,void*buf,int len){
  if (fd < 0 || current->fd_table[fd] < 0) return -1;
  int s_idx = current->fd_table[fd];
  OpenFile *of = &system_open_table[s_idx];
  Finfo *f = &file_table[of->file_idx];

  // 特殊设备文件
  if (f->write != NULL) {
    size_t result = f->write(buf,of->open_offset, len);
    of->open_offset += result;
    return result; 
  }

  if (of->open_offset >= f->inode.size) return 0;
  if (of->open_offset + len > f->inode.size) {
    len = f->inode.size - of->open_offset; 
  }
  //TODO copy mkfs.c中的分配函数来实现文件的写入，当前设计中只能写入已经分配的盘块。
  size_t to_write = len;
  size_t current_off = of->open_offset;
  while (to_write > 0) {
    uint32_t l_block = current_off / BSIZE;
    uint32_t b_offset = current_off % BSIZE;
    uint32_t p_block = bmap(f, l_block);

    size_t can_write = BSIZE - b_offset;
    size_t actual_write = (to_write < can_write) ? to_write : can_write;

    ramdisk_write((char *)buf + (len - to_write), p_block * BSIZE + b_offset, actual_write);

    current_off += actual_write;
    to_write -= actual_write;
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
