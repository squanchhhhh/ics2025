#include <stdlib.h>
#include <stdint.h>
#include "/home/parallels/ics2025/nanos-lite/include/fs.h"

struct superblock {
  uint32_t magic;      // 校验 
  uint32_t size;       // 磁盘总块数 (2048)
  uint32_t nblocks;    // 数据块数量
  uint32_t ninodes;    // Inode 总数 (1024)
  uint32_t inodestart; // Inode Table 的起始块号
  uint32_t bmapstart;  // Data Bitmap 的起始块号
};
#define NDIRECT 12

//磁盘inode结构
struct dinode {
  uint16_t type;              // 文件类型: 1(目录), 2(普通文件), 3(设备)
  uint16_t nlink;             // 链接数
  uint32_t size;              // 文件大小 (Bytes)
  uint32_t addrs[NDIRECT + 1]; // 物理块号映射表: 12个直接块 + 1个间接块
  uint32_t pad[3];            // 填充位
};

//目录结构
#define DIRSIZ 14
struct dirent {
  uint16_t inum;       // 该文件对应的 Inode 编号
  char name[DIRSIZ];   // 文件名
};

