#include <elf.h>
#include <stdlib.h>
#include <trace/elf.h>

char *elf_buf = NULL;
static Func funcs[FUNC_NUM];
static int nr_func = 0;
static bool elf_loaded = false;

// 内部私有函数：加载文件到内存
static void load_elf_to_buf(const char *path) {
  FILE *fp = fopen(path, "rb");
  Assert(fp, "Can not open '%s'", path);

  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  
  elf_buf = (char *)malloc(size);
  fseek(fp, 0, SEEK_SET);
  int ret = fread(elf_buf, size, 1, fp);
  assert(ret == 1);
  fclose(fp);
  Log("The elf is %s, size = %ld", path, size);
}

// 内部私有函数：解析内存中的 ELF 数据
static void parse_elf_internal() {
  Elf32_Ehdr *ehdr = (Elf32_Ehdr *)elf_buf;
  Elf32_Shdr *shdrs = (Elf32_Shdr *)(elf_buf + ehdr->e_shoff);
  
  // 获取节表字符串表地址
  const char *shstrtab = elf_buf + shdrs[ehdr->e_shstrndx].sh_offset;

  int sym_index = 0;
  int str_link = 0;

  // 1. 寻找 .symtab 和对应的 .strtab
  for (int i = 0; i < ehdr->e_shnum; i++) {
    const char *name = shstrtab + shdrs[i].sh_name;
    if (strcmp(name, ".symtab") == 0) {
      sym_index = i;
      str_link = shdrs[i].sh_link;
    }
  }

  if (sym_index == 0) {
    Log("No .symtab found, ftrace will be disabled.");
    return;
  }

  // 2. 提取符号表
  Elf32_Shdr *symtab_sh = &shdrs[sym_index];
  Elf32_Sym *symtab = (Elf32_Sym *)(elf_buf + symtab_sh->sh_offset);
  int nr_sym = symtab_sh->sh_size / symtab_sh->sh_entsize;

  // 3. 提取字符串表
  const char *strtab = elf_buf + shdrs[str_link].sh_offset;

  // 4. 遍历并记录函数符号
  nr_func = 0;
  for (int i = 0; i < nr_sym; i++) {
    if (ELF32_ST_TYPE(symtab[i].st_info) == STT_FUNC && symtab[i].st_size > 0) {
      funcs[nr_func].valid = true;
      funcs[nr_func].begin = symtab[i].st_value;
      funcs[nr_func].end = symtab[i].st_value + symtab[i].st_size;
      strncpy(funcs[nr_func].name, strtab + symtab[i].st_name, 63);
      funcs[nr_func].name[63] = '\0';
      
      // Log("Found func: %s at [0x%x, 0x%x]", funcs[nr_func].name, funcs[nr_func].begin, funcs[nr_func].end);
      nr_func++;
      if (nr_func >= FUNC_NUM) break;
    }
  }
  elf_loaded = true;
}

// 公共接口：初始化
void init_elf(const char *path) {
  if (path == NULL) return;
  load_elf_to_buf(path);
  parse_elf_internal();
}

// 公共接口：根据地址查 ID
int elf_find_func_by_addr(vaddr_t addr) {
  for (int i = 0; i < nr_func; i++) {
    if (funcs[i].valid && addr >= funcs[i].begin && addr < funcs[i].end) {
      return i;
    }
  }
  return -1;
}

// 公共接口：根据 ID 获取函数指针
Func* elf_get_func_by_id(int id) {
  if (id >= 0 && id < nr_func) return &funcs[id];
  return NULL;
}

bool is_elf_loaded() { return elf_loaded; }