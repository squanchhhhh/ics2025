#ifndef __ELF_H__
#define __ELF_H__

#include <common.h>

// 函数符号结构体
typedef struct {
  char name[64];
  vaddr_t begin;
  vaddr_t end;
  bool valid;
} Func;

#define FUNC_NUM 1024

// 初始化接口：由 monitor 调用
void init_elf(const char *path);

// 查询接口：根据地址查找函数 ID
int elf_find_func_by_addr(vaddr_t addr);

// 数据获取接口：根据 ID 获取函数详情
Func* elf_get_func_by_id(int id);

// 状态接口：检查 ELF 是否成功加载
bool is_elf_loaded();

#endif // __ELF_H__