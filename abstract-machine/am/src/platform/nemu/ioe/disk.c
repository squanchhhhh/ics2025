#include <am.h>
#include <nemu.h>

// 对应我们在 NEMU 中定义的寄存器偏移
#define DISK_BLOCK_REG (DISK_ADDR + 0)
#define DISK_MEM_REG   (DISK_ADDR + 4)
#define DISK_CTRL_REG  (DISK_ADDR + 8)

void __am_disk_config(AM_DISK_CONFIG_T *cfg) {
  cfg->blksz = 4096; 
  cfg->blkcnt = 8192;
  cfg->present = true;
}

void __am_disk_status(AM_DISK_STATUS_T *stat) {
  stat->ready = (inl(DISK_CTRL_REG) == 0);
}

void __am_disk_blkio(AM_DISK_BLKIO_T *io) {
  uint8_t *buffer = (uint8_t *)io->buf;
  
  for (uint32_t i = 0; i < io->blkcnt; i++) {
    outl(DISK_BLOCK_REG, io->blkno + i);
    
    outl(DISK_MEM_REG, (uintptr_t)buffer + i * 4096);
    
    outl(DISK_CTRL_REG, io->write ? 1 : 0);
    
    while (inl(DISK_CTRL_REG) != 0);
  }
}