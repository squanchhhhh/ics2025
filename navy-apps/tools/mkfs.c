
#include "fs_shared.h"
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
extern void fs_init_sb(struct superblock *external_sb);
extern uint32_t fs_create_path(const char *path, int16_t type);
extern int iget(int inum, struct dinode *inode);
extern size_t inode_write(struct dinode *ip, const void *src, uint32_t offset, uint32_t n);
extern void inode_update(int i, struct dinode *ip);

static int ramdisk_fd = -1;
void disk_read(void *buf, uint32_t block_num) {
    //printf("try to read disk num %d\n",block_num);
    lseek(ramdisk_fd, block_num * BSIZE, SEEK_SET);
    size_t rc = read(ramdisk_fd, buf, BSIZE);
     (void)rc;
    //printf("read size %ld\n",rc);
}

void disk_write(const void *buf, uint32_t block_num) {
    //printf("try to write disk num %d\n",block_num);
    lseek(ramdisk_fd, block_num * BSIZE, SEEK_SET);
    size_t rc = write(ramdisk_fd,buf, BSIZE);
    (void)rc;
    //printf("write size %ld\n",rc);
}

void read_host_file(){
    int fd = open("./ramdisk.img",O_RDWR | O_CREAT, 0664);
    ramdisk_fd = fd;
}

void init_fs(){
    sb.magic = 0x20010124;
    sb.bmap_start = 2;
    sb.bmap_data_start = 1024;
    sb.inode_start = 3;
    sb.data_start = 19;
    sb.root_inum = 1;
    sb.ninodes = 1024;
    sb.nblocks = 8173;
    sb.root_inum = 1;
    sb.size = 8192;
    char buf[BSIZE];
    memcpy(buf, &sb, sizeof(struct superblock));
    disk_write(buf, 1);

    // 初始化位图 (Block 2)
    memset(buf, 0, BSIZE);
    // 标记 Inode 0 和 Inode 1 (根目录) 已用
    set_bit(buf, 0); 
    set_bit(buf, 1); 
    // 标记前 19 个数据块已用 (从 bmap_data_start 开始)
    for(int i = 0; i < 19; i++) {
        set_bit(buf, sb.bmap_data_start + i);
    }
    disk_write(buf, sb.bmap_start);

    // 初始化 Inode Table 并写入根目录 Inode
    memset(buf, 0, BSIZE);
    struct dinode *inode_table = (struct dinode *)buf;
    struct dinode root = {0};
    root.type = TYPE_DIR;
    root.size = sizeof(struct dirent);
    root.addrs[0] = sb.data_start; 
    inode_table[sb.root_inum] = root;
    disk_write(buf, sb.inode_start);

    // 初始化目录内容
    memset(buf, 0, BSIZE);
    struct dirent *de = (struct dirent *)buf;
    de->inum = sb.root_inum;
    strcpy(de->name, ".");
    disk_write(buf, root.addrs[0]);
    printf("File system initialized successfully.\n");
}
int main(int argc, char *argv[]) {
    read_host_file();
    
    // 自动加载/初始化超级块
    char buf[BSIZE];
    disk_read(buf, 1);
    struct superblock *temp_sb = (struct superblock *)buf;

    if (temp_sb->magic != 0x20010124) {
        init_fs();
        disk_read(buf, 1); // 初始化后再读一次
    }
    memcpy(&sb, temp_sb, sizeof(struct superblock));

    // 如果运行 ./mkfs <src_path> <dst_path>
    if (argc == 3) {
        const char *host_path = argv[1];
        const char *fs_path = argv[2];

        // 1. 读取 host 文件
        FILE *f = fopen(host_path, "rb");
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        void *file_buf = malloc(fsize);
        size_t n = fread(file_buf, 1, fsize, f);
        (void)n;
        fclose(f);

        // 2. 在文件系统中创建文件
        uint32_t inum = fs_create_path(fs_path, TYPE_FILE);
        struct dinode ip;
        iget(inum, &ip);

        // 3. 写入内容并更新 Inode
        inode_write(&ip, file_buf, 0, fsize);
        inode_update(inum, &ip);

        free(file_buf);
        printf("Successfully added %s to filesystem at %s\n", host_path, fs_path);
    }
    //printf("\n--- File System Tree ---\n/\n");
    //fs_tree(1, 1); 
    close(ramdisk_fd);
    return 0;
}