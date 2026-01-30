#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define BSIZE 4096
#define DISK_BLOCKS 8192
#define MAX_INODES 1024
#define NDIRECT 12
#define DIRSIZ 14
#define MAX_FILE 64

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

struct dirent {
    uint16_t inum;
    char name[DIRSIZ];
};

void bread(int fd, uint32_t block_num, void *buf) {
    lseek(fd, block_num * BSIZE, SEEK_SET);
    if (read(fd, buf, BSIZE) != BSIZE) {
        perror("bread error");
    }
}

void bwrite(int fd, uint32_t block_num, void *buf) {
    lseek(fd, block_num * BSIZE, SEEK_SET);
    if (write(fd, buf, BSIZE) != BSIZE) {
        perror("bwrite error");
    }
}

uint32_t balloc(int img_fd, struct superblock *sb) {
    unsigned char bitmap[BSIZE];
    bread(img_fd, sb->bmap_start, bitmap);

    for (uint32_t i = 0; i < BSIZE; i++) {
        if (bitmap[i] == 0xFF) continue;
        for (int j = 0; j < 8; j++) {
            if (!(bitmap[i] & (1 << j))) {
                uint32_t block_num = i * 8 + j;
                bitmap[i] |= (1 << j);
                bwrite(img_fd, sb->bmap_start, bitmap);
                return block_num;
            }
        }
    }
    return 0;
}

uint32_t ialloc(int img_fd, struct superblock *sb, short type) {
    unsigned char buf[BSIZE];
    bread(img_fd, sb->inode_start, buf);
    struct dinode *inodes = (struct dinode *)buf;

    // 0号保留，1号是root，所以从2开始分配给普通文件
    for (uint32_t i = 2; i < MAX_FILE; i++) {
        if (inodes[i].type == TYPE_NONE) {
            inodes[i].type = type;
            bwrite(img_fd, sb->inode_start, buf);
            return i;
        }
    }
    return 0;
}


void add_to_root(int img_fd, struct superblock *sb, uint16_t inum, char *filename) {
    unsigned char inode_buf[BSIZE];
    bread(img_fd, sb->inode_start, inode_buf);
    struct dinode *root = &((struct dinode *)inode_buf)[sb->root_inum];

    if (root->size == 0) {
        root->addrs[0] = balloc(img_fd, sb);
    }

    unsigned char data_buf[BSIZE];
    bread(img_fd, root->addrs[0], data_buf);

    struct dirent *de = (struct dirent *)data_buf;
    int slot = root->size / sizeof(struct dirent);
    
    de[slot].inum = inum;
    strncpy(de[slot].name, filename, DIRSIZ);

    root->size += sizeof(struct dirent);

    bwrite(img_fd, root->addrs[0], data_buf);
    bwrite(img_fd, sb->inode_start, inode_buf);
}

//初始化文件系统
uint32_t create_img() {
    int fd = open("ramdisk.img", O_RDWR | O_CREAT | O_TRUNC, 0664);
    ftruncate(fd, DISK_BLOCKS * BSIZE);

    struct superblock sp = {
        .magic = 0x20010124,
        .size = DISK_BLOCKS,
        .nblocks = 8173,
        .ninodes = 1024,
        .bmap_start = 2,
        .inode_start = 3,
        .data_start = 19,
        .root_inum = 1
    };

    unsigned char buf[BSIZE] = {0};
    memcpy(buf, &sp, sizeof(sp));
    bwrite(fd, 1, buf);

    memset(buf, 0, BSIZE);
    for (int i = 0; i < 19; i++) buf[i / 8] |= (1 << (i % 8));
    bwrite(fd, sp.bmap_start, buf);

    memset(buf, 0, BSIZE);
    struct dinode *inodes = (struct dinode *)buf;
    inodes[sp.root_inum].type = TYPE_DIR;
    inodes[sp.root_inum].nlink = 1;
    inodes[sp.root_inum].size = 0;
    bwrite(fd, sp.inode_start, buf);

    close(fd);
    printf("Image created successfully.\n");
    return 0;
}

uint32_t add_file(char *filename, char *alias) {
    struct superblock sp;
    int img_fd = open("ramdisk.img", O_RDWR);
    
    unsigned char sb_buf[BSIZE];
    bread(img_fd, 1, sb_buf);
    memcpy(&sp, sb_buf, sizeof(sp));

    int src_fd = open(filename, O_RDONLY);
    struct stat st;
    fstat(src_fd, &st);
    
    uint32_t nblock = (st.st_size + BSIZE - 1) / BSIZE;
    uint32_t inum = ialloc(img_fd, &sp, TYPE_FILE);

    unsigned char inode_buf[BSIZE];
    bread(img_fd, sp.inode_start, inode_buf);
    struct dinode *dip = &((struct dinode *)inode_buf)[inum];
    dip->size = st.st_size;

    //只考虑直接情况，间接暂时不实现
    for (uint32_t i = 0; i < nblock; i++) {
        uint32_t bno = balloc(img_fd, &sp);
        dip->addrs[i] = bno;

        unsigned char file_data[BSIZE] = {0};
        read(src_fd, file_data, BSIZE);
        bwrite(img_fd, bno, file_data);
    }

    bwrite(img_fd, sp.inode_start, inode_buf);

    add_to_root(img_fd, &sp, inum, alias);

    printf("Added %s as /%s (inum: %d, size: %lld)\n", filename, alias, inum, (long long)st.st_size);

    close(src_fd);
    close(img_fd);
    return inum;
}

int main(int argc, char *argv[]) {
    if (argc < 2) return -1;

    if (strcmp(argv[1], "-i") == 0) {
        create_img();
    } else if (strcmp(argv[1], "-a") == 0 && argc == 4) {
        add_file(argv[2], argv[3]);
    }
    
    return 0;
}