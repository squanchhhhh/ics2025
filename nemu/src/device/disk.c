/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/
#include <assert.h>
#include <device/map.h>
#include <utils.h>

#define DISK_BASE 0x40000000
#define DISK_SIZE 0x2000000  // 32MB 
static FILE *disk_fp = NULL;
static uint8_t *disk_space = NULL;

static void disk_io_handler(uint32_t addr, int len, bool is_write) {
  uint32_t offset = addr - DISK_BASE;
  if (disk_fp == NULL) return;

  if (fseek(disk_fp, offset, SEEK_SET) != 0) return;

  if (is_write) {
    int i = fwrite(disk_space + offset, len, 1, disk_fp);
    assert(i==0);
    fflush(disk_fp); // 确保立即写入 Ubuntu 磁盘
  } else {
    int i = fread(disk_space + offset, len, 1, disk_fp);
    assert(i==0);
  }
}

void init_disk() {
  const char *path = getenv("DISK_IMG");
  if (path == NULL) {
    Log("DISK_IMG not set, disk device disabled.");
    return;
  }

  disk_fp = fopen(path, "r+b");
  Assert(disk_fp, "Cannot open disk image at %s", path);

  disk_space = (uint8_t *)malloc(DISK_SIZE);
  Assert(disk_space, "Malloc disk space failed");

  memset(disk_space, 0, DISK_SIZE);
  add_mmio_map("disk", DISK_BASE, disk_space, DISK_SIZE, disk_io_handler);

  Log("Real-world Disk simulated at [0x%08x, 0x%08x] using %s", 
      DISK_BASE, DISK_BASE + DISK_SIZE - 1, path);
}