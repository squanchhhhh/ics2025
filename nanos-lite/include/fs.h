#ifndef __FS_H__
#define __FS_H__

#include <common.h>
typedef size_t (*ReadFn) (void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn) (const void *buf, size_t offset, size_t len);
#define BSIZE 4096

//磁盘上的索引节点
#define NDIRECT 12
struct dinode {
  uint16_t type;               // 文件类型:  1(目录), 2(普通文件), 3(设备)
  uint16_t nlink;              // 链接数
  uint32_t size;               // 文件大小 (Bytes)
  uint32_t addrs[NDIRECT + 1]; // 12个直接块=48KB + 1个间接块=4KB/4B * 4KB=4MB = 4MB+48KB 
  uint32_t pad[3];             // 填充位
};
//超级块
struct superblock {
  uint32_t magic;      // 校验 
  uint32_t size;       // 磁盘总块数 (2048)
  uint32_t nblocks;    // 数据块数量
  uint32_t ninodes;    // Inode 总数 (1024)
  uint32_t inodestart; // Inode Table 的起始块号
  uint32_t bmapstart;  // Data Bitmap 的起始块号
};
//目录结构
#define DIRSIZ 14
struct dirent {
  uint16_t inum;       
  char name[DIRSIZ];   
};

//打开文件表
#define MAX_OPEN_INODES 16
typedef struct {
  uint32_t inum;
  uint32_t size;
  uint32_t addrs[NDIRECT + 1];
  int ref_count; 
} VFS_Inode;
VFS_Inode inode_table[MAX_OPEN_INODES];

//
#define MAX_FD 16
typedef struct {
  bool used;
  VFS_Inode *ptr_inode;
  uint32_t open_offset; 
} F_Desc;
F_Desc fd_table[MAX_FD];

typedef struct {
  char *name;          
  size_t open_offset;  // 读写offset
  bool used;           // 是否有进程在使用文件
  struct dinode inode; // 磁盘inode
  ReadFn read;
  WriteFn write;
} Finfo;

#ifndef SEEK_SET
enum {SEEK_SET, SEEK_CUR, SEEK_END};
#endif

#endif
