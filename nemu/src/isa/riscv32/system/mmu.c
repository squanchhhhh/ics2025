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
  paddr_t pg_dir_base = (cpu.csr.satp & 0x3fffff) << 12;

  uint32_t vpn1 = (vaddr >> 22) & 0x3ff;
  uint32_t vpn0 = (vaddr >> 12) & 0x3ff;
  uint32_t offset = vaddr & 0xfff;

  paddr_t pde_addr = pg_dir_base + vpn1 * 4;
  uint32_t pde = paddr_read(pde_addr, 4);

  assert(pde & 0x1); 

  paddr_t pg_tab_base = (pde >> 10) << 12;
  paddr_t pte_addr = pg_tab_base + vpn0 * 4;
  uint32_t pte = paddr_read(pte_addr, 4);

  assert(pte & 0x1);

  paddr_t pa = ((pte >> 10) << 12) | offset;

  return pa;
}