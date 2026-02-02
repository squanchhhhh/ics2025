#include <am.h>
#include <nemu.h>

void __am_disk_config(AM_DISK_CONFIG_T *cfg) {
  cfg->blksz = 4096;
  cfg->blkcnt = 8192;
  cfg->present = true;
}

void __am_disk_status(AM_DISK_STATUS_T *stat) {
  stat->ready = true;        
}

void __am_disk_blkio(AM_DISK_BLKIO_T *io) {
  uint8_t *p_disk = (uint8_t *)(DISK_ADDR + io->blkno * 512);

  if (io->write) {
    memcpy(p_disk, io->buf, io->blkcnt * 512);
  } else {
    memcpy(io->buf, p_disk, io->blkcnt * 512);
  }
}