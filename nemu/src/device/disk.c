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
#include <device/map.h>
#include <utils.h>
#include <memory/paddr.h>

#define DISK_BLOCK_REG 0  
#define DISK_MEM_REG   4  
#define DISK_CTRL_REG  8  
#define DISK_BLOCK_SIZE 4096

static uint32_t disk_regs[3]; 
static FILE *disk_fp = NULL;

static void handle_disk_transfer() {
  uint32_t blk_no  = disk_regs[DISK_BLOCK_REG / 4];
  uint32_t mem_ptr = disk_regs[DISK_MEM_REG / 4];
  uint32_t cmd     = disk_regs[DISK_CTRL_REG / 4];

  void *host_ptr = guest_to_host(mem_ptr);
  
  if (fseek(disk_fp, blk_no * DISK_BLOCK_SIZE, SEEK_SET) != 0) {
    Log("Disk Error: fseek failed");
    return;
  }

  if (cmd == 0) { // Read
    if (fread(host_ptr, DISK_BLOCK_SIZE, 1, disk_fp) != 1) Log("Disk Read Failed");
  } else if (cmd == 1) { // Write
    if (fwrite(host_ptr, DISK_BLOCK_SIZE, 1, disk_fp) != 1) Log("Disk Write Failed");
    fflush(disk_fp);
  }

  // 状态回滚：设置为 0 表示 Ready
  disk_regs[DISK_CTRL_REG / 4] = 0;
}
static void disk_io_handler(uint32_t addr, int len, bool is_write) {
  uint32_t offset = addr - CONFIG_DISK_CTL_MMIO;
  if (is_write && offset == DISK_CTRL_REG && len == 4) {
    handle_disk_transfer();
  }
}

void init_disk() {
  const char *path = getenv("DISK_IMG");
  if (path == NULL) return;

  disk_fp = fopen(path, "r+b");
  Assert(disk_fp, "Cannot open disk image at %s", path);

  add_mmio_map("disk", CONFIG_DISK_CTL_MMIO, disk_regs, 12, disk_io_handler);

  Log("Block Device simulated via MMIO at [0x%08x]", CONFIG_DISK_CTL_MMIO);
}