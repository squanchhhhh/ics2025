#ifndef __FS_H__
#define __FS_H__

#include <common.h>
typedef size_t (*ReadFn) (void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn) (const void *buf, size_t offset, size_t len);
#define BSIZE 4096

#define BSIZE 4096
#define DISK_BLOCKS 8192
#define MAX_INODES 1024
#define NDIRECT 12
#define DIRSIZ 14
#define MAX_FILE 64
#define MAX_NR_PROC_FILE 32

struct dirent {
  uint16_t inum;       
  char name[DIRSIZ];   
};

enum file_type { TYPE_NONE, TYPE_FILE, TYPE_DIR };

struct superblock {
    uint32_t magic;
    uint32_t size;        // 总块数
    uint32_t nblocks;     // 数据块数
    uint32_t ninodes;     // Inode总数
    uint32_t bmap_start;
    uint32_t inode_start;
    uint32_t data_start;
    uint32_t root_inum;
};
struct dinode {
    short type;
    short major;
    short minor;
    short nlink;
    uint32_t size;
    uint32_t addrs[NDIRECT + 1];
};

typedef struct {
  char *name;          
  size_t open_offset;  // 读写offset
  bool used;           // 是否有进程在使用文件
  struct dinode inode; // 磁盘inode
  ReadFn read;
  WriteFn write;
} Finfo;

extern Finfo file_table[];
#ifndef SEEK_SET
enum {SEEK_SET, SEEK_CUR, SEEK_END};
#endif

#endif
