#include "common.h"
#include "debug.h"
#include "fs_shared.h"
#include "proc.h"
#include <fs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "device.h"

Finfo file_table[MAX_MEM_INODES] __attribute__((used)) = {
  /*
  [FD_STDIN]  = { .name = "stdin",  .read = invalid_read, .write = invalid_write },
  [FD_STDOUT] = { .name = "stdout", .read = invalid_read, .write = serial_write },
  [FD_STDERR] = { .name = "stderr", .read = invalid_read, .write = serial_write },
  {.name="/dev/serial",  .read = invalid_read, .write = serial_write}, 
  {.name="/dev/fb",.read = invalid_read, .write = fb_write},   
  {.name="/dev/events",  .read = events_read,  .write = invalid_write}, 
  {.name="/proc/dispinfo",  .read = dispinfo_read, .write = invalid_write}, 
  { .name = NULL }
   */
};

// -- fs-disk
void disk_read(void *buf, uint32_t block_num) {
    ramdisk_read(buf, block_num * BSIZE, BSIZE);
}

void disk_write(const void *buf, uint32_t block_num) {
    ramdisk_write(buf, block_num * BSIZE, BSIZE);
}

int mount_root_fs(){
  char buf[BSIZE];
  disk_read(buf, 1);
  struct superblock *temp_sb = (struct superblock *)buf;
  if (temp_sb->magic != 0x20010124) {
    Log("Failed to mount: Unknown file system magic 0x%x", temp_sb->magic);
    return -1;
  }
  memcpy(&sb, temp_sb, sizeof(struct superblock));
  //printf("FileSystem Info: root=%d, inode_start=%d, IPB=%d\n", 
  //sb.root_inum, sb.inode_start, IPB);
  Log("FileSystem mounted successfully.");
  return 0;
}
// -- fs-device
size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}
size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}
static int nr_device = 0;

static struct {
  const char *name;
  size_t (*read)(void *, size_t, size_t);
  size_t (*write)(const void *, size_t, size_t);
} dev_configs[] = {
  {"/dev/serial",  invalid_read, serial_write},
  {"/dev/fb",      invalid_read, fb_write},
  {"/dev/events",  events_read,  invalid_write},
  {"/proc/dispinfo", dispinfo_read, invalid_write},
};

void mount_devtmpfs() {
  int n = sizeof(dev_configs) / sizeof(dev_configs[0]);
  for (int i = 0; i < n; i++) {
    file_table[i].name = strdup(dev_configs[i].name);
    file_table[i].read  = dev_configs[i].read;
    file_table[i].write = dev_configs[i].write;
    file_table[i].ref   = 1;          
    file_table[i].inum  = 0xFFFFFFFF; 
    if (strcmp(file_table[i].name, "/dev/fb") == 0) {
       AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
       file_table[i].inode.size = cfg.width * cfg.height * 4;
    }
  }
  nr_device = n; 
  Log("VFS: %d devices registered in devtmpfs.", nr_device);
}

//增加设备
int vfs_register_device(const char *name, void *read_fn, void *write_fn) {
  if (nr_device >= MAX_MEM_INODES) return -1;
  int idx = nr_device++;
  file_table[idx].name = strdup(name);
  file_table[idx].read = read_fn;
  file_table[idx].write = write_fn;
  file_table[idx].ref = 1;
  file_table[idx].inum = 0xFFFFFFFF;
  return 0;
}

// -- fs-vfs
enum {FD_STDIN, FD_STDOUT, FD_STDERR, FD_FB};

void init_fs(){
  mount_root_fs();
  mount_devtmpfs();

  // 初始化系统打开文件表
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    system_open_table[i].used = false;
  }
  // 为内核打开stdin stdout stderr
  int fd0 = vfs_open("/dev/serial", 0); // stdin
  int fd1 = vfs_open("/dev/serial", 0); // stdout
  int fd2 = vfs_open("/dev/serial", 0); // stderr
  
  Log("Kernel standard I/O initialized: fd[%d, %d, %d]", fd0, fd1, fd2);
}

int find_or_alloc_finfo(uint32_t inum, const char *path) {
  for (int i = nr_device; i < MAX_MEM_INODES; i++) {
    if (file_table[i].ref > 0 && file_table[i].inum == inum) {
      file_table[i].ref++; 
      return i;
    }
  }
  for (int i = nr_device; i < MAX_MEM_INODES; i++) {
    if (file_table[i].ref == 0) {
      file_table[i].inum = inum;
      file_table[i].ref = 1;
      if (file_table[i].name) free(file_table[i].name);
      file_table[i].name = strdup(path); 
      iget(inum, &file_table[i].inode); 
      //printf("load file %s to system\n",path);
      return i;
    }
  }
  return -1; 
}
void f_put(int f_idx) {
    if (f_idx < 0 || f_idx >= MAX_MEM_INODES) return;
    Finfo *f = &file_table[f_idx];
    //跳过设备文件
    if (f->inum == 0xFFFFFFFF) {
        return; 
    }
    if (f->ref > 0) {
        f->ref--;
        if (f->ref == 0) {
            Log("VFS: Releasing memory inode for %s (inum: %d)", f->name, f->inum);
            f->name = NULL;  
            f->inum = 0;
            memset(&f->inode, 0, sizeof(struct dinode));
        }
    }
}
//--
OpenFile system_open_table[MAX_OPEN_FILES];
int alloc_system_fd(int f_idx, int flags) {
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (!system_open_table[i].used) {
      system_open_table[i].used = 1;
      system_open_table[i].file_idx = f_idx;
      system_open_table[i].flags = flags;
      if (flags & O_APPEND) {
        system_open_table[i].open_offset = file_table[f_idx].inode.size;
      } else {
        system_open_table[i].open_offset = 0;
      }
      //printf("alloc sys_open_table idx %d, point to fdx %d, name in file_table is %s\n",i,f_idx,file_table[f_idx].name);
      file_table[f_idx].ref++; 
      return i;
    }
  }
  return -1;
}
void free_system_fd(int s_idx){
  OpenFile *of = &system_open_table[s_idx];
  file_table[of->file_idx].ref--;
  of->used = false;
}

//--
int vfs_open(const char *path, int flags) {
    int f_idx = -1;
    for (int i = 0; i < MAX_MEM_INODES; i++) {
        if (file_table[i].name != NULL && strcmp(path, file_table[i].name) == 0) {
            f_idx = i; 
            break; 
        }
    }
    if (f_idx == -1) {
        uint32_t inum = namei(path);
        if (inum == 0) return -1;
        f_idx = find_or_alloc_finfo(inum, path);
    }
    if (f_idx < 0) return -1;
    //printf("vfs_open: path %s matched file_table idx %d, now allocating s_idx...\n", path, f_idx);
    return alloc_system_fd(f_idx, flags);
}

int fs_open(const char *path, int flags, mode_t mode) {
    //printf("try to open file %s\n",path);
    (void)mode;
    int s_idx = vfs_open(path, flags);
    if (s_idx < 0) return -1;
    system_open_table[s_idx].open_offset = 0;
    int fd = map_to_proc_fd(s_idx);
    //printf("s_idx = %d\n",s_idx);
    //printf("system_open_table[s_idx].file_idx = %d\n",system_open_table[s_idx].file_idx);
    Log("[Syscall] Process '%s' mapped System Index %d (%s) to local FD %d", 
            current->name, s_idx,file_table[system_open_table[s_idx].file_idx].name, fd);
    return fd; 
}

size_t vfs_read(int s_idx, void *buf, size_t len) {
    if (s_idx < 0 || s_idx >= MAX_OPEN_FILES) return -1;
    OpenFile *of = &system_open_table[s_idx];
    Finfo *f = &file_table[of->file_idx];
    size_t n = 0;

    if (f->read) { 
        n = f->read(buf, of->open_offset, len);
    } else {      
        if (of->open_offset >= f->inode.size) return 0;
        size_t readable = f->inode.size - of->open_offset;
        if (len > readable) len = readable;
        n = inode_read(&f->inode, buf, of->open_offset, len);
    }
    of->open_offset += n;
    //printf("read done\n");
    return n;
}
size_t fs_read(int fd, void *buf, size_t len) {
    int s_idx = current->fd_table[fd];
   // Log("Reading fd = %d, file = %s, len = %d", fd, file_table[system_open_table[s_idx].file_idx].name, len);
    return vfs_read(s_idx, buf, len);
}

size_t vfs_write(int s_idx, const void *buf, size_t len) {
    if (s_idx < 0 || s_idx >= MAX_OPEN_FILES || !system_open_table[s_idx].used) return -1;
    OpenFile *of = &system_open_table[s_idx];
    Finfo *f = &file_table[of->file_idx];
    size_t n = 0;

    if (f->write != NULL) {
        n = f->write(buf, of->open_offset, len);
    } else {
        size_t max_size = (NDIRECT + NINDIRECT) * BSIZE; 
        if (of->open_offset + len > max_size) len = max_size - of->open_offset;
        if (len <= 0) return 0;
        n = inode_write(&f->inode, buf, of->open_offset, len);
        if (of->open_offset + n > f->inode.size) f->inode.size = of->open_offset + n;
    }
    of->open_offset += n;
    return n;
}
size_t fs_lseek(int fd, size_t offset, int whence) {
  if (fd < 0 || fd >= MAX_NR_PROC_FILE) return -1;
  int s_idx = current->fd_table[fd];
  if (s_idx < 0) return -1;
  return vfs_lseek(s_idx, offset, whence);
}
size_t fs_write(int fd, const void *buf, size_t len) {
    if (fd < 0 || fd >= MAX_NR_PROC_FILE) {
        return -1;
    }
    int s_idx = current->fd_table[fd];

    if (s_idx < 0) {
        return -1;
    }
    return vfs_write(s_idx, buf, len);
}

size_t vfs_lseek(int s_idx, size_t offset, int whence) {
    OpenFile *of = &system_open_table[s_idx];
    Finfo *f = &file_table[of->file_idx];
    size_t new_offset = of->open_offset;

    switch (whence) {
        case SEEK_SET: new_offset = offset; break;
        case SEEK_CUR: new_offset += offset; break;
        case SEEK_END: new_offset = f->inode.size + offset; break;
        default: return -1;
    }
    if (new_offset < 0) return -1;
    
    of->open_offset = new_offset;
    return of->open_offset;
}


void vfs_close(int s_idx) {
  if (s_idx < 0 || s_idx >= MAX_OPEN_FILES) return;
  
  OpenFile *of = &system_open_table[s_idx];
  if (!of->used) return;

  file_table[of->file_idx].ref--;
  of->used = 0; 
  of->open_offset = 0; 
}


int fs_close(int fd) {
  if (fd < 0 || fd >= MAX_NR_PROC_FILE) {
    return -1;
  }
  int s_idx = current->fd_table[fd];
  if (s_idx < 0) {
    return -1; 
  }

  vfs_close(s_idx);

  current->fd_table[fd] = -1;

  return 0;
}