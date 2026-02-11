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

/*
功能：挂载根文件系统，识别磁盘格式并初始化全局超级块信息
1. 从磁盘的第 1 号块（通常为超级块所在位置）读取元数据到临时缓冲区
2. 校验超级块中的魔数 (Magic Number)，确认磁盘格式是否为系统支持的类型
3. 若校验通过，将超级块元数据拷贝至内核全局变量 sb 中，供后续 Inode 寻址使用
4. 初始化完成后打印挂载成功日志，建立内核与磁盘文件系统的逻辑连接
*/
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
/*
功能：在系统启动时挂载设备文件系统 (devtmpfs)，将硬件设备映射到文件表
1. 根据 dev_configs 数组配置，遍历并初始化 file_table 的前 nr_device 项
2. 为每个设备分配唯一的名称，并绑定对应的硬件读写钩子函数 (read/write)
3. 设置设备文件的初始引用计数为 1，并统一标记 Inode 号为 0xFFFFFFFF 以区分磁盘文件
4. 特殊处理图形显示设备 (/dev/fb)：通过硬件接口读取屏幕参数并计算映射文件的大小
*/
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
  memset(system_open_table, 0, sizeof(OpenFile) * MAX_OPEN_FILES);
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

/*
功能：将磁盘文件的 Inode 信息加载到内存文件表 (file_table) 中
1. 遍历文件表（跳过设备项），根据 Inode 号 (inum) 检查文件是否已在内存中
2. 若命中且引用计数 ref > 0，直接返回现有表项下标，实现文件对象共享
3. 若未命中，寻找一个 ref 为 0 的空闲表项，初始化 Inode 号、文件名等信息
4. 调用 iget 从磁盘同步 Inode 元数据到内存，并将初始 ref 设为 0
*/
int find_or_alloc_finfo(uint32_t inum, const char *path) {
    for (int i = nr_device; i < MAX_MEM_INODES; i++) {
        if (file_table[i].ref > 0 && file_table[i].inum == inum) {
            return i;
        }
    }
    for (int i = nr_device; i < MAX_MEM_INODES; i++) {
        if (file_table[i].ref == 0) {
            file_table[i].inum = inum;
            file_table[i].ref = 0; 
            if (file_table[i].name) free(file_table[i].name);
            file_table[i].name = strdup(path); 
            iget(inum, &file_table[i].inode); 
            return i;
        }
    }
    return -1;
}

/*
功能：回收内存文件表项 (Finfo) 占用的资源
1. 校验 f_idx 合法性，并跳过设备文件 (inum 为 0xFFFFFFFF)
2. 检查引用计数 ref 是否确实为 0，防止误删正在使用的文件
3. 释放文件名占用的堆内存 (strdup 分配的空间)
4. 清空文件元数据 (Inode) 与 Inode 号，将槽位标记为空闲
*/
void f_put(int f_idx) {
    if (f_idx < 0 || f_idx >= MAX_MEM_INODES) return;
    Finfo *f = &file_table[f_idx];
    if (f->inum == 0xFFFFFFFF) return; 

    if (f->ref == 0) {
        if (f->name) { free(f->name); f->name = NULL; }
        f->inum = 0;
        f->read = NULL; 
        f->write = NULL;
        memset(&f->inode, 0, sizeof(struct dinode));
    }
}
//--
OpenFile system_open_table[MAX_OPEN_FILES];
/*
功能：在系统打开文件表(system_open_table)中分配槽位并绑定物理文件
1. 遍历系统打开文件表，寻找 used 为 false 的空闲项
2. 记录物理文件索引(f_idx)及打开标志(flags)
3. 根据 O_APPEND 标志初始化文件偏移量(open_offset)
4. 增加物理文件(file_table)的引用计数 ref，确保文件在使用期间不被卸载
*/
int alloc_system_fd(int f_idx, int flags) {
  if (f_idx < 0 || f_idx >= MAX_MEM_INODES) return -1;
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
      file_table[f_idx].ref++; 
      printf("VFS: Allocated sys_open_table[%d] -> file_table[%d] (%s), ref=%d\n",
              i, f_idx, file_table[f_idx].name, file_table[f_idx].ref);
      return i;
    }
  }
  printf("VFS: Global system_open_table overflow!\n");
  return -1;
}

/*
功能：释放全局系统打开文件表项并递减物理文件引用
1. 校验 s_idx 合法性及该项是否确实在使用中
2. 获取对应的物理文件索引 f_idx，并递减其引用计数 ref
3. 标记系统表项 used 为 false，彻底释放该句柄
4. 若物理文件引用计数归零且非设备文件，调用 f_put 回收内存资源
*/
void free_system_fd(int s_idx) {
  if (s_idx < 0 || s_idx >= MAX_OPEN_FILES || !system_open_table[s_idx].used) {
    return;
  }
  OpenFile *of = &system_open_table[s_idx];
  int f_idx = of->file_idx;
  if (f_idx >= 0 && f_idx < MAX_MEM_INODES) {
    file_table[f_idx].ref--;
    if (file_table[f_idx].ref == 0 && file_table[f_idx].inum != 0xFFFFFFFF) {
       f_put(f_idx); 
    }
  }
  of->used = false;
  of->file_idx = -1;
  of->open_offset = 0;
}

//--
/*
功能：将文件的dinode放入finfo并在file_table分配空间
1.遍历file_table查看是否有已经打开的同名文件
2.使用namei从文件系统获取path对应的inum
3.调用find_or_alloc_finfo创建finfo并加入file_table
4.调用alloc_system_fd将fdx记录到打开文件表中
*/
int vfs_open(const char *path, int flags) {
    for (int i = 0; i < nr_device; i++) {
        if (file_table[i].name != NULL && strcmp(path, file_table[i].name) == 0) {
            return alloc_system_fd(i, flags);
        }
    }
    uint32_t inum = namei(path);
    if (inum == 0) return -1;
    int f_idx = find_or_alloc_finfo(inum, path);
    if (f_idx < 0) return -1;

    return alloc_system_fd(f_idx, flags);
}
/*
功能：用户程序打开文件的系统调用接口，建立进程 FD 与系统 s_idx 的映射
1. 调用 vfs_open 获取全局系统打开文件表下标 s_idx
2. 若 s_idx 成功分配，调用 map_to_proc_fd 在当前进程文件表中寻找空位映射为局部 fd
3. 若映射 fd 失败（进程文件表满），调用 vfs_close 回滚已申请的系统资源并返回错误
4. 映射成功后初始化文件偏移量，并记录日志，返回供用户程序使用的文件描述符 fd
*/
int fs_open(const char *path, int flags, mode_t mode) {
    (void)mode;
    int s_idx = vfs_open(path, flags);
    if (s_idx < 0) return -1;
    int fd = map_to_proc_fd(s_idx);
    if (fd < 0) {
        vfs_close(s_idx); 
        return -1;
    }
    Log("[Syscall] Process '%s' mapped System Index %d (%s) and file idx %d to local FD %d", 
            current->name, s_idx, file_table[system_open_table[s_idx].file_idx].name,system_open_table[s_idx].file_idx,fd);
    return fd; 
}
/*
功能：内核级文件读取接口，根据文件类型执行具体读取操作
1. 校验 s_idx 合法性及系统表项是否有效 (used)
2. 判断文件类型：若存在自定义 read（设备文件），则调用该函数
3. 若为普通文件，根据 inode.size 限制读取长度，防止读取越界
4. 调用 inode_read 从磁盘读取数据，并同步增加全局文件偏移量 open_offset
*/
size_t vfs_read(int s_idx, void *buf, size_t len) {
    if (s_idx < 0 || s_idx >= MAX_OPEN_FILES || !system_open_table[s_idx].used) {
        return -1; 
    }
    OpenFile *of = &system_open_table[s_idx];
    Finfo *f = &file_table[of->file_idx];
    size_t n = 0;
    if (f->read) { 
        n = f->read(buf, of->open_offset, len);
    } 
    else {      
        if (of->open_offset >= f->inode.size) return 0;
        size_t readable = f->inode.size - of->open_offset;
        if (len > readable) len = readable;
        n = inode_read(&f->inode, buf, of->open_offset, len);
    }
    of->open_offset += n;
    return n;
}
/*
功能：用户级读取系统调用接口，将进程 FD 映射为内核 s_idx
1. 校验进程文件描述符 fd 的范围合法性
2. 从当前进程 fd_table 中获取对应的系统打开表索引 s_idx
3. 校验 s_idx 是否有效，若未绑定文件则返回错误
4. 调用 vfs_read 执行具体的内核读取逻辑并返回读取字节数
*/
size_t fs_read(int fd, void *buf, size_t len) {
    if (fd < 0 || fd >= MAX_NR_PROC_FILE) return -1;
    int s_idx = current->fd_table[fd];
    if (s_idx < 0) return -1;
    return vfs_read(s_idx, buf, len);
}

/*
功能：内核级文件写入接口，支持设备钩子与普通文件 Inode 写入
1. 校验 s_idx 合法性。若为 O_APPEND 模式，写入前强制将偏移量设为文件末尾
2. 判断文件类型：若为设备文件，调用 f->write；若为普通文件，执行 Inode 写入
3. 写入时进行磁盘最大容量检查，防止超出 NDIRECT + NINDIRECT 的限制
4. 写入完成后更新 inode.size（若文件增长）并同步更新 open_offset
*/
size_t vfs_write(int s_idx, const void *buf, size_t len) {
    if (s_idx < 0 || s_idx >= MAX_OPEN_FILES || !system_open_table[s_idx].used) return -1;
    OpenFile *of = &system_open_table[s_idx];
    Finfo *f = &file_table[of->file_idx];
    if (of->flags & O_APPEND) {
        of->open_offset = f->inode.size;
    }
    size_t n = 0;
    //printf("call f.write = %p f.name = %s\n",f->write,f->name);
    if (f->write != NULL) {
        n = f->write(buf, of->open_offset, len);
    } else {
        size_t max_size = (NDIRECT + NINDIRECT) * BSIZE; 
        if (of->open_offset + len > max_size) len = max_size - of->open_offset;
        if (len <= 0) return 0;

        n = inode_write(&f->inode, buf, of->open_offset, len);
        if (of->open_offset + n > f->inode.size) {
            f->inode.size = of->open_offset + n;
        }
    }
    of->open_offset += n;
    return n;
}
/*
功能：用户级写入系统调用接口，执行 FD 映射与基础合法性校验
1. 校验进程 fd 范围，通过当前进程 fd_table 获取系统表索引 s_idx
2. 校验 s_idx 指向的文件是否有效
3. 调用 vfs_write 执行内核写入操作
*/
size_t fs_write(int fd, const void *buf, size_t len) {
    if (fd < 0 || fd >= MAX_NR_PROC_FILE) return -1;
    int s_idx = current->fd_table[fd];
    if (s_idx < 0) return -1;
    return vfs_write(s_idx, buf, len);
}

/*
功能：内核级偏移量调整接口，支持设置、增量及末尾偏移
1. 校验 s_idx 合法性。使用有符号数计算 new_offset 以防止负数越界
2. 根据 whence (SEEK_SET/CUR/END) 计算目标偏移位置
3. 更新全局文件表的 open_offset，并返回调整后的新位置
*/
size_t vfs_lseek(int s_idx, size_t offset, int whence) {
    if (s_idx < 0 || s_idx >= MAX_OPEN_FILES || !system_open_table[s_idx].used) return -1;
    OpenFile *of = &system_open_table[s_idx];
    Finfo *f = &file_table[of->file_idx];
    
    // 使用有符号长整型防止计算溢出或负值
    ssize_t new_offset = of->open_offset;

    switch (whence) {
        case SEEK_SET: new_offset = offset; break;
        case SEEK_CUR: new_offset += (ssize_t)offset; break;
        case SEEK_END: new_offset = (ssize_t)f->inode.size + (ssize_t)offset; break;
        default: return -1;
    }

    if (new_offset < 0) return -1;
    
    of->open_offset = (size_t)new_offset;
    return of->open_offset;
}

/*
功能：用户级偏移量调整接口，将进程 FD 映射为内核 s_idx
1. 校验进程 fd 范围及映射有效性
2. 调用 vfs_lseek 更新系统级文件偏移量并返回结果
*/
size_t fs_lseek(int fd, size_t offset, int whence) {
    if (fd < 0 || fd >= MAX_NR_PROC_FILE) return -1;
    int s_idx = current->fd_table[fd];
    if (s_idx < 0) return -1;
    return vfs_lseek(s_idx, offset, whence);
}


/*
功能：内核级文件关闭接口，释放系统打开文件表项
1. 校验 s_idx 合法性及是否在用，防止重复释放
2. 递减物理文件 (file_table) 的引用计数 ref
3. 若引用计数归零且非设备文件，调用 f_put 彻底回收内存 Inode 及文件名资源
4. 清空全局系统表项的 used 标志与偏移量，释放句柄控制权
*/
void vfs_close(int s_idx) {
  if (s_idx < 0 || s_idx >= MAX_OPEN_FILES) return;
  OpenFile *of = &system_open_table[s_idx];
  if (!of->used) return;
  int f_idx = of->file_idx;
  if (f_idx >= 0 && f_idx < MAX_MEM_INODES) {
    file_table[f_idx].ref--;
    if (file_table[f_idx].ref == 0) {
      printf("file_table idx %d (%s) delete due to ref == 0\n",f_idx,file_table[f_idx].name);
      f_put(f_idx);
    }
  }
  of->used = 0; 
  of->file_idx = -1;
  of->open_offset = 0; 
}

/*
功能：用户级关闭系统调用接口，解除进程 FD 与内核映射
1. 校验进程文件描述符 fd 的范围合法性
2. 从进程 fd_table 中获取对应的系统表索引 s_idx
3. 若 s_idx 有效，调用 vfs_close 执行内核级资源回收
4. 将进程 fd_table 对应项重置为 -1，使该 FD 可被后续 open 复用
*/
int fs_close(int fd) {
  if (fd >= 0 && fd <= 2) return 0; //禁止关闭stdio
  printf("fs_close fd = %d\n",fd);
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