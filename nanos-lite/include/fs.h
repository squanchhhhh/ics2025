#ifndef __FS_H__
#define __FS_H__
#include "fs_shared.h"
#include <common.h>
typedef size_t (*ReadFn) (void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn) (const void *buf, size_t offset, size_t len);


//---系统内存inode
#define MAX_MEM_INODES 64
#define STATIC_DEVICE 3 //stdin stdout stderr
typedef struct {
  char *name;       
  uint32_t inum;   
  struct dinode inode; 
  int ref;
  ReadFn read;
  WriteFn write;
} Finfo; 

extern Finfo file_table[]; 

void init_fs();                                 // 初始化 file_table
int find_or_alloc_finfo(uint32_t inum, const char *path); // 查找或分配内存 Inode
void f_put(int f_idx);                        // 减少引用计数，必要时回收

//---应用和系统打开文件表
#define MAX_OPEN_FILES 1024 //所有进程打开文件小于1024
typedef struct {
  int file_idx;       // 指向静态 file_table 的下标
  size_t open_offset; // 为每次open使用独立的偏移量
  bool used;          // 槽位是否被占用
  int flags;
} OpenFile;

extern OpenFile system_open_table[]; 

int alloc_system_fd(int f_idx, int flags);    // 分配并初始化 OpenFile
void free_system_fd(int s_idx);               // 释放 OpenFile 槽位


//--通用操作函数
enum {O_APPEND};
int vfs_open(const char *path, int flags);
int sys_open(const char *path, int flags);
size_t vfs_read(int s_idx, void *buf, size_t len);
size_t fs_read(int fd, void *buf, size_t len);
size_t vfs_write(int s_idx, const void *buf, size_t len);
size_t fs_write(int fd, const void *buf, size_t len);
size_t vfs_lseek(int s_idx, size_t offset, int whence);
size_t fs_lseek(int fd, size_t offset, int whence);
void vfs_close(int s_idx);
void fs_close(int fd);                        
void fstate(int fd, struct dinode *d); 
#include <sys/types.h> 
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif
#endif
