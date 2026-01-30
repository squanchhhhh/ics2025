#ifndef __FS_H__
#define __FS_H__

#include <common.h>
typedef size_t (*ReadFn) (void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn) (const void *buf, size_t offset, size_t len);

#define BSIZE 4096
#define NDIRECT 12
#define DIRSIZ 14
#define MAX_NR_PROC_FILE 32
#define MAX_OPEN_FILES 1024
#define MAX_STATIC_FILE 64
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
  struct dinode inode; // 磁盘inode
  int ref;
  ReadFn read;
  WriteFn write;
} Finfo;

extern Finfo file_table[];  //静态文件表

typedef struct {
  int file_idx;       // 指向静态 file_table 的下标
  size_t open_offset; // 为每次open掉哟个使用独立的偏移量
  bool used;          // 槽位是否被占用
} OpenFile;

OpenFile system_open_table[MAX_OPEN_FILES]; // 系统打开文件表


#ifndef SEEK_SET
enum {SEEK_SET, SEEK_CUR, SEEK_END};
#endif

#endif
