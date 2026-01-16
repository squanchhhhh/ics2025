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
#include "local-include/reg.h"

const char *regs[] = {
  "$0", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
  "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
  "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
  "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
};

void isa_reg_display() {
  for (int i = 0; i < 32; i++) {
    printf("gpr[%d](%s) = 0x%x\n", i, regs[i], gpr(i));
  }
  printf("mstatus: 0x%08x  mtvec:   0x%08x\n", cpu.csr.mstatus, cpu.csr.mtvec);
  printf("mepc:    0x%08x  mcause:  0x%08x\n", cpu.csr.mepc, cpu.csr.mcause);
}

word_t isa_reg_str2val(const char *s, bool *success) {
  for(int i = 0; i < 32; i++){
    if(!strcmp(s, regs[i])){
      *success = true;
      return gpr(i);
    }
  }
  if (strcmp(s, "mstatus") == 0) return cpu.csr.mstatus;
  if (strcmp(s, "mepc") == 0)    return cpu.csr.mepc;
  if (strcmp(s, "mtvec") == 0)   return cpu.csr.mtvec;
  if (strcmp(s, "mcause") == 0)  return cpu.csr.mcause;
  *success = false;
  return 0;
}

word_t csr_read(uint32_t addr) {
  switch (addr) {
    case 0x300: return cpu.csr.mstatus;
    case 0x305: return cpu.csr.mtvec;
    case 0x341: return cpu.csr.mepc;
    case 0x342: return cpu.csr.mcause;
    default: panic("Unimplemented CSR read at addr 0x%x", addr);
  }
}

void csr_write(uint32_t addr, word_t data) {
  switch (addr) {
    case 0x300: cpu.csr.mstatus = data; break;
    case 0x305: cpu.csr.mtvec = data; break;
    case 0x341: cpu.csr.mepc = data; break;
    case 0x342: cpu.csr.mcause = data; break;
    default: panic("Unimplemented CSR write at addr 0x%x", addr);
  }
}