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

#include <isa.h>
#include <memory/vaddr.h>
#include <memory/paddr.h>
/*
功能：将虚拟地址翻译成物理地址
1. 从 satp 寄存器获取一级页目录的物理基地址 (PPN)
2. 提取虚拟地址的索引
3. 一级页表查询 (Page Directory)
4. 二级页表查询 (Page Table)
5. 拼接最终的物理地址
*/
paddr_t isa_mmu_translate(vaddr_t vaddr, int len, int type) {
  word_t satp = cpu.csr.satp;
  if (!(satp & 0x80000000)) return MEM_RET_OK; 

  paddr_t pg_dir_base = (satp & 0x3fffff) << 12;

  uint32_t vpn1 = (vaddr >> 22) & 0x3ff;
  paddr_t pte1_addr = pg_dir_base + vpn1 * 4;
  word_t pte1 = paddr_read(pte1_addr, 4);

  assert(pte1 & 0x1); 

  paddr_t pg_tab_base = (pte1 >> 10) << 12;
  uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
  paddr_t pte0_addr = pg_tab_base + vpn0 * 4;
  word_t pte0 = paddr_read(pte0_addr, 4);

  assert(pte0 & 0x1);

  paddr_t paddr = ((pte0 >> 10) << 12) | (vaddr & 0xfff);
  return paddr;
}