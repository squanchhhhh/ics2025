#ifndef __FS_SHARED_H__
#define __FS_SHARED_H__

#include <stdint.h>  
#include <stddef.h>  
#include <string.h>


#define BSIZE 4096
#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint32_t))
#define DIRSIZ 30
enum file_type { TYPE_NONE, TYPE_FILE, TYPE_DIR };

#define IPB (BSIZE / sizeof(struct dinode))
#define DPB (BSIZE / sizeof(struct dirent))
#define GET_BUF (char*)malloc(BSIZE)

struct dirent {
  uint16_t inum;       
  char name[DIRSIZ];   
};
/*
struct superblock {
  uint32_t magic;        // 0x20010124
  uint32_t size;         // 总块数：32MB/4KB = 8192
  uint32_t nblocks;      // 数据块数量：8173
  uint32_t ninodes;      // Inode 总数：1024
  
  // 起始地址
  uint32_t bmap_start;   // 位图起始块号：2 前128字节作为inode位图，后面为data位图
  uint32_t inode_start;  // Inode Table 起始块号：3
  uint32_t data_start;   // 数据区起始块号：19
  
  uint32_t root_inum;    // 根目录的 Inode 号
}; total 9 * 4B = 36B*/
struct superblock {
    uint32_t magic;
    uint32_t size;
    uint32_t nblocks;
    uint32_t ninodes;
    uint32_t bmap_start;
    uint32_t bmap_data_start;
    uint32_t inode_start;
    uint32_t data_start;
    uint32_t root_inum;
};
extern struct superblock sb;
struct dinode {
    int16_t type;     
    int16_t major;
    int16_t minor;
    int16_t nlink;
    uint32_t size;
    uint32_t addrs[NDIRECT + 1];
};
// 全局超级块快照，供 bmap, balloc, ialloc 等函数使用
extern struct superblock sb;

/**
 * @brief 逻辑块号转物理块号 (bmap)
 * @param ip    内存中的磁盘 inode 指针
 * @param bn    文件内的逻辑块号 (0, 1, 2...)
 * @param alloc 如果物理块不存在，是否分配 (1为分配, 0为只读查询)
 * @return 返回物理块号，失败或不存在返回 0
 */
uint32_t bmap(struct dinode *ip, uint32_t bn, int alloc);

void fs_tree(uint32_t inum, int indent);
/**
 * @brief 从指定的 Inode 读取数据
 * @param ip     Inode 指针
 * @param dst    目标缓冲区
 * @param offset 文件内偏移量
 * @param n      读取字节数
 * @return 实际读取的字节数
 */
size_t inode_read(struct dinode *ip, void *dst, uint32_t offset, uint32_t n);

/**
 * @brief 向指定的 Inode 写入数据
 * @param ip     Inode 指针
 * @param src    源数据缓冲区
 * @param offset 文件内偏移量
 * @param n      写入字节数
 * @return 实际写入的字节数
 */
size_t inode_write(struct dinode *ip, const void *src, uint32_t offset, uint32_t n);

/**
 * @brief 分配一个空闲的数据块
 * @return 返回分配到的物理块号
 */
uint32_t balloc();

/**
 * @brief 分配一个空闲的 Inode 节点
 * @param type 文件类型 (TYPE_FILE 或 TYPE_DIR)
 * @return 返回分配到的 Inode 编号 (inum)
 */
uint32_t ialloc(int16_t type);

/**
 * @brief 在给定目录 Inode 下查找名称匹配的 entry
 * @param dp    目录的 Inode 指针
 * @param name  要查找的文件名
 * @return 返回找到的 Inode 编号，未找到返回 0
 */
uint32_t dir_lookup(struct dinode *dp, const char *name);

/**
 * @brief 向给定目录 Inode 写入一个新的 entry
 * @param dp    目录的 Inode 指针
 * @param name  新文件名
 * @param inum  对应的新 Inode 编号
 * @return 成功返回 0，失败返回 -1
 */
int dir_link(struct dinode *dp, const char *name, uint32_t inum);

/**
 * @brief 在指定路径下创建一个新的 Inode（文件或目录）
 * @param path  文件的完整路径（如 /bin/hello）
 * @param type  TYPE_FILE 或 TYPE_DIR
 * @return uint32_t 返回新创建的 Inode 编号，失败返回 0
 */
uint32_t fs_create_node(const char *path, int16_t type);

int get_bit(void *buf, int index);
void set_bit(void *buf, int index);

// 从磁盘根据 inum 读取 dinode 结构到内存
int iget(int inum, struct dinode *inode);

// 将内存中的 dinode 结构同步回磁盘对应 inum 的位置
void inode_update(int i, struct dinode *ip);

// 从 ip 指向的文件/目录中读取数据（处理多块及逻辑块映射）
size_t inode_read(struct dinode *ip, void *dst, uint32_t offset, uint32_t n);

// 向 ip 指向的文件/目录写入数据（处理块分配和大小更新）
size_t inode_write(struct dinode *ip, const void *src, uint32_t offset, uint32_t n);

// 解析路径字符串（如 "/bin/hello"），返回对应的 Inode 编号。失败返回 0
uint32_t namei(const char *path);

// 在给定目录 dp 下查找名为 name 的文件，返回其 Inode 编号
uint32_t dir_lookup(struct dinode *dp, const char *name);

// 路径工具函数：解析路径中的下一个元素（如 "/a/b" -> 返回 "a"，剩余 "/b"）
const char* skipelem(const char *path, char *name);

// 递归打印文件系统结构（用于调试和展示）
void fs_tree(uint32_t inum, int indent);

// 自动解析并创建路径（如果中间目录不存在则自动创建），返回目标 Inode
uint32_t fs_create_path(const char *path, int16_t type);

// 在目录 dp 下建立一个到 inum 的链接，名称为 name
int dir_link(struct dinode *dp, const char *name, uint32_t inum);

// 分配一个新的 Inode 并初始化类型
uint32_t ialloc(int16_t type);
#endif