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

int bread(char * buf,int offset){
  return ramdisk_read(buf,offset*BSIZE,BSIZE);
}
int bwrite(char * buf,int offset){
  return ramdisk_write(buf,offset*BSIZE,BSIZE);
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
  return 0;
 // int fd = add_fd_to_current_proc(sys_fd);

//return fd; 
}

//读文件
size_t fs_read(int fd, void *buf, size_t len){
  int s_idx = current->fd_table[fd];
  OpenFile *of = &system_open_table[s_idx];

  Finfo *f = &file_table[of->file_idx];

  //非普通文件
  if (f->read != NULL) {
    return f->read(buf, 0, len); 
  }

  return 0;
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
    
    strcpy(f->name, dir_entries[i].name);
    
    get_dinode(dir_entries[i].inum, &f->inode);
    
    f->read = NULL;  
    f->write = NULL; 
    f->ref = 1;
    Log("load file %s.", f->name);
    nr_static_file++;
  }

  free(dir_entries); 
  Log("FS initialized. %d files loaded.", nr_static_file);
}
