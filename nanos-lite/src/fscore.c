#include "fs_shared.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern void disk_read(void *buf, uint32_t block_num);
extern void disk_write(const void *buf, uint32_t block_num);

extern uint32_t balloc();
struct superblock sb;
static void _bzero(uint32_t block_num) {
    static char buf[BSIZE];
    memset(buf, 0, BSIZE);
    disk_write(buf, block_num);
}
/**
 * @brief 逻辑块号转物理块号 (bmap)
 * @param ip    内存中的磁盘 inode 指针
 * @param bn    文件内的逻辑块号 (0, 1, 2...)
 * @param alloc 如果物理块不存在，是否分配 (1为分配, 0为只读查询)
 * @return 返回物理块号，失败或不存在返回 0
 */
uint32_t bmap(struct dinode *ip, uint32_t bn, int alloc) {
    uint32_t addr;

    if (bn < NDIRECT) {
        addr = ip->addrs[bn];
        if (addr == 0) {
            if (!alloc) return 0;
            ip->addrs[bn] = addr = balloc();
        }
        return addr;
    }
    bn -= NDIRECT;
    if (bn < NINDIRECT) {
        uint32_t indirect_conf_addr = ip->addrs[NDIRECT];
        if (indirect_conf_addr == 0) {
            if (!alloc) return 0;
            ip->addrs[NDIRECT] = indirect_conf_addr = balloc();
            _bzero(indirect_conf_addr);
        }
        static uint32_t indirect_table[BSIZE / sizeof(uint32_t)];
        disk_read(indirect_table, indirect_conf_addr);
        addr = indirect_table[bn];
        if (addr == 0) {
            if (!alloc) return addr;
            indirect_table[bn] = addr = balloc();
            disk_write(indirect_table, indirect_conf_addr);
        }
        return addr;
    }
    return 0;
}
/**
 * @param is_write : 1 为写, 0 为读
 */
static size_t inode_transfer(struct dinode *ip, void *buf, uint32_t offset, uint32_t n, int is_write) {
    uint32_t total_done = 0;
    if (!is_write) {
        if (offset >= ip->size) return 0;
        if (offset + n > ip->size) n = ip->size - offset;
    }
    while (total_done < n) {
        uint32_t curr_pos = offset + total_done;
        uint32_t l_block  = curr_pos / BSIZE;
        uint32_t b_offset = curr_pos % BSIZE;
        uint32_t can_do = BSIZE - b_offset;
        if (can_do > (n - total_done)) can_do = n - total_done;
        uint32_t p_block = bmap(ip, l_block, is_write);
        static char temp_buf[BSIZE]; 
        if (is_write) {
            if (b_offset == 0 && can_do == BSIZE) {
                disk_write(buf + total_done, p_block);
            } else {
                disk_read(temp_buf, p_block);
                memcpy(temp_buf + b_offset, buf + total_done, can_do);
                disk_write(temp_buf, p_block);
            }
        } else {
            if (p_block == 0) {
                memset(buf + total_done, 0, can_do);
            } else {
                if (b_offset == 0 && can_do == BSIZE) {
                    disk_read(buf + total_done, p_block);
                } else {
                    disk_read(temp_buf, p_block);
                    memcpy(buf + total_done, temp_buf + b_offset, can_do);
                }
            }
        }
        total_done += can_do;
    }
    if (is_write && (offset + total_done > ip->size)) {
        ip->size = offset + total_done;
    }

    return total_done;
}
size_t inode_read(struct dinode *ip, void *dst, uint32_t offset, uint32_t n) {
    return inode_transfer(ip, dst, offset, n, 0);
}

size_t inode_write(struct dinode *ip, const void *src, uint32_t offset, uint32_t n) {
    return inode_transfer(ip, (void *)src, offset, n, 1);
}
// 检查 buf 指向的位图中，第 index 位是否为 1
int get_bit(void *buf, int index) {
    uint8_t *ptr = (uint8_t *)buf;
    int byte_idx = index / 8;
    int bit_off  = index % 8;
    return (ptr[byte_idx] & (1 << bit_off)) != 0;
}

// 将 buf 指向的位图中，第 index 位置为 1
void set_bit(void *buf, int index) {
    uint8_t *ptr = (uint8_t *)buf;
    int byte_idx = index / 8;
    int bit_off  = index % 8;
    ptr[byte_idx] |= (1 << bit_off);
}

void inode_update(int i,struct dinode *ip){
    static struct dinode buf[IPB];
    int block_offset = i/IPB + sb.inode_start;
    disk_read(buf, block_offset);
    int inode_offset = i%IPB;
    buf[inode_offset] = *ip;
    disk_write(buf, block_offset);
}

uint32_t ialloc(int16_t type) {
    static char buf[BSIZE];
    disk_read(buf, sb.bmap_start);

    for (int i = 0; i < sb.ninodes; i++) {
        if (get_bit(buf, i) == 0) {
            set_bit(buf, i);
            disk_write(buf, sb.bmap_start); 
            struct dinode ip;
            memset(&ip, 0, sizeof(ip));
            ip.type = type;
            inode_update(i, &ip); 

            return i; 
        }
    }
    printf("ialloc: no free inodes\n");
    return 0;
}

uint32_t balloc() {
    static char buf[BSIZE];
    disk_read(buf, sb.bmap_start);
    for (int i = 1024; i < sb.nblocks + 1024; i++) {
        if (get_bit(buf, i) == 0) {
            set_bit(buf, i);
            disk_write(buf, sb.bmap_start);
            uint32_t p_block = sb.data_start + (i - 1024);
            _bzero(p_block);
            return p_block;
        }
    }
    printf("balloc: no free blocks\n");
    return 0;
}

uint32_t dir_lookup(struct dinode *dp, const char *name) {
    //printf("[DEBUG] dir_lookup: name='%s', inode_size=%d, inode_type=%d\n", name, dp->size, dp->type);
    
    if (dp->type != TYPE_DIR) {
        printf("[DEBUG] dir_lookup: FAILED because type %d is not a directory\n", dp->type);
        return 0;
    }
    static struct dirent buf[DPB];
    uint32_t offset = 0;
    while (offset < dp->size) {
        uint32_t bytes_to_read = sizeof(buf);
        if (offset + bytes_to_read > dp->size) {
            bytes_to_read = dp->size - offset;
        }
        int ret = inode_read(dp, buf, offset, bytes_to_read);
        if (ret <= 0) {
            printf("[DEBUG] inode_read returned %d at offset %d, dp->size is %d\n", ret, offset, dp->size);
            break; 
        }
        int num_entries = bytes_to_read / sizeof(struct dirent);
        for (int i = 0; i < num_entries; i++) {
            if (buf[i].inum != 0 && strcmp(buf[i].name, name) == 0) {
                //printf("finded dir name %s in inum %d\n",name,buf[i].inum);
                return buf[i].inum;
            }
        }
        offset += bytes_to_read;
    }
    //printf("cannot find dir %s\n",name);
    return 0; 
}

int dir_link(struct dinode *dp, const char *name, uint32_t inum) {
    if (dir_lookup(dp, name) != 0) {
        return -1; 
    }
    static struct dirent de;
    uint32_t off;
    for (off = 0; off < dp->size; off += sizeof(struct dirent)) {
        if (inode_read(dp, &de, off, sizeof(struct dirent)) != sizeof(struct dirent)) {
            break;
        }
        if (de.inum == 0) {
            break;
        }
    }
    strncpy(de.name, name, DIRSIZ);
    de.name[DIRSIZ - 1] = '\0';
    de.inum = inum;
    if (inode_write(dp, &de, off, sizeof(struct dirent)) != sizeof(struct dirent)) {
        return -1;
    }
    printf("add dirent name %s\n",name);
    return 0;
}

int iget(int inum,struct dinode * inode){
    static struct dinode buf[IPB];
    int block_offset = inum /IPB + sb.inode_start;
    int inode_offset = inum %IPB;
    disk_read(buf, block_offset);
    memcpy(inode, &buf[inode_offset], sizeof(struct dinode));
    return 0;
}
const char* skipelem(const char *path, char *name) {
    while (*path == '/') path++;
    if (*path == 0) return NULL;
    
    const char *s = path;
    while (*path != '/' && *path != 0) path++;
    
    int len = path - s;
    if (len >= DIRSIZ) len = DIRSIZ - 1;
    memcpy(name, s, len);
    name[len] = 0;
    
    while (*path == '/') path++;
    return path;
}
uint32_t namei(const char *path) {
    char name[DIRSIZ];
    uint32_t inum = 1; 
    struct dinode curr_inode;
    if (iget(inum, &curr_inode) < 0) return 0;
    while ((path = skipelem(path, name)) != NULL) {
        printf("path %s\n",path);
        printf("name %s\n",name);
        uint32_t next_inum = dir_lookup(&curr_inode, name);
        printf("next_inum %d\n",next_inum);
        if (next_inum == 0) return 0; 
        inum = next_inum;
        if (iget(inum, &curr_inode) < 0) return 0;
    }
    return inum;
}

/**
 * @brief 路径解析并自动逐级创建目录
 * @param path 完整路径，如 "/share/fonts/hello.txt"
 * @param type 最终目标的类型 (TYPE_FILE 或 TYPE_DIR)
 * @return uint32_t 最终目标的 Inode 编号
 */
uint32_t fs_create_path(const char *path, int16_t type) {
    char name[DIRSIZ];
    uint32_t curr_inum = 1; 
    struct dinode curr_ip;
    
    const char *next_p;
    const char *curr_p = path;

    while ((next_p = skipelem(curr_p, name)) != NULL) {
        if (iget(curr_inum, &curr_ip) < 0) return 0;
        
        uint32_t found_inum = dir_lookup(&curr_ip, name);
        
        if (found_inum == 0) {
            int16_t new_type;
            if (*next_p == '\0') {
                new_type = type;
            } else {
                new_type = TYPE_DIR;
            }

            found_inum = ialloc(new_type);
            if (dir_link(&curr_ip, name, found_inum) < 0) return 0;
            inode_update(curr_inum, &curr_ip); 
            if (new_type == TYPE_DIR) {
                struct dinode new_ip;
                iget(found_inum, &new_ip);
                dir_link(&new_ip, ".", found_inum);
                dir_link(&new_ip, "..", curr_inum);
                inode_update(found_inum, &new_ip);
            }
        }
        
        curr_inum = found_inum;
        curr_p = next_p;
        if (*curr_p == '\0') break;
    }
    printf("create path %s, last inode %d \n",path,curr_inum);
    return curr_inum;
}

/**
 * @brief 递归打印文件系统树状结构
 * @param inum 当前处理的 Inode 编号
 * @param indent 递归深度（用于打印空格缩进）
 */
void fs_tree(uint32_t inum, int indent) {
    struct dinode ip;
    if (iget(inum, &ip) < 0) return;

    if (ip.type != TYPE_DIR) return;

    struct dirent de;
    uint32_t off;
    for (off = 0; off < ip.size; off += sizeof(struct dirent)) {
        if (inode_read(&ip, &de, off, sizeof(struct dirent)) != sizeof(struct dirent))
            break;

        if (de.inum == 0) continue; 
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) continue;

        for (int i = 0; i < indent; i++) printf("  ");
        
        struct dinode child_ip;
        iget(de.inum, &child_ip);
        
        if (child_ip.type == TYPE_DIR) {
            printf("\033[1;34m%s/\033[0m\n", de.name); 
            fs_tree(de.inum, indent + 1);            
        } else {
            printf("%s  \033[0;90m(%d bytes)\033[0m\n", de.name, child_ip.size); 
        }
    }
}