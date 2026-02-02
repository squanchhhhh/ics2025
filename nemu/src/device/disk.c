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
#include <stdio.h>
#include <utils.h>
#include <memory/paddr.h>

#define DISK_BLOCK_REG 0  
#define DISK_MEM_REG   4  
#define DISK_CTRL_REG  8  
#define DISK_BLOCK_SIZE 4096

static uint32_t disk_regs[3]; 
static FILE *disk_fp = NULL;

static void handle_disk_transfer() {
    printf("call handle_disk_transfer\n");
  uint32_t blk_no  = disk_regs[DISK_BLOCK_REG / 4];
  uint32_t mem_ptr = disk_regs[DISK_MEM_REG / 4];
  uint32_t cmd     = disk_regs[DISK_CTRL_REG / 4];

  // 强制打印，确认读取到的参数
  printf("NEMU DISK OP: blk=%d, mem=0x%08x, cmd=%d\n", blk_no, mem_ptr, cmd);

  void *host_ptr = guest_to_host(mem_ptr);
  if (fseek(disk_fp, blk_no * DISK_BLOCK_SIZE, SEEK_SET) != 0) {
    perror("fseek");
    return;
  }

  if (cmd == 0) { // Read
    if (fread(host_ptr, DISK_BLOCK_SIZE, 1, disk_fp) != 1) {
      printf("Disk Read Error at blk %d\n", blk_no);
    }
  } else { // Write
    fwrite(host_ptr, DISK_BLOCK_SIZE, 1, disk_fp);
    fflush(disk_fp);
  }
}
static uint32_t *disk_base = NULL; // 模仿串口，改用指针
static void disk_io_handler(uint32_t offset, int len, bool is_write) {
  // 模仿串口：不管 len 到底是多少，先看是不是写了 CTRL 寄存器
  if (is_write && offset == DISK_CTRL_REG) {
    // 此时数据已经由 mmio_write 填入 disk_base[2] 了（如果是先填数据后调用的逻辑）
    // 或者我们需要手动同步
    handle_disk_transfer();
  }
}
void init_disk() {
  // 1. 获取镜像路径
  const char *path = getenv("DISK_IMG");
  if (path == NULL) return;
  disk_fp = fopen(path, "r+b");
  Assert(disk_fp, "Cannot open disk image");

  // 2. 模仿串口：申请空间
  disk_base = (uint32_t *)new_space(12); // 3个寄存器 * 4字节

  // 3. 注册：注意最后一个参数是回调函数
  add_mmio_map("disk", CONFIG_DISK_CTL_MMIO, disk_base, 12, disk_io_handler);
}

