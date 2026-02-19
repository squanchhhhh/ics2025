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
#include <memory/paddr.h>

word_t vaddr_ifetch(vaddr_t addr, int len) {
  paddr_t paddr = isa_mmu_translate(addr, len, MEM_TYPE_IFETCH);
  if (paddr == MEM_RET_OK) return paddr_read(addr, len); 
  return paddr_read(paddr, len);
}

word_t vaddr_read(vaddr_t addr, int len) {
  paddr_t paddr = isa_mmu_translate(addr, len, MEM_TYPE_READ);
  if (paddr == MEM_RET_OK) return paddr_read(addr, len);
  return paddr_read(paddr, len);
}

void vaddr_write(vaddr_t addr, int len, word_t data) {
  // --- 拦截开始 ---
  // 监控虚拟地址 0x7ffff38 附近（即你报错的 Context 地址）
  if (addr >= 0x7ffff000 && addr <= 0x7fffffff) {
      // 这里的 addr 是虚拟地址，我们可以直接判断
      printf("\n[VAddr Write Hit!] VA: 0x%08x | Data: 0x%08x | PC: 0x%08x\n", 
              addr, data, cpu.pc);
      
      // 如果你想在写的一瞬间就停下来用 GDB 调试：
      // if (data == 0x17171717) assert(0); 
  }
  // --- 拦截结束 ---

  paddr_t paddr = isa_mmu_translate(addr, len, MEM_TYPE_WRITE);
  if (paddr == MEM_RET_OK) {
      paddr_write(addr, len, data);
  } else {
      paddr_write(paddr, len, data);
  }
}