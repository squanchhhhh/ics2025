/***************************************************************************************
 * Copyright (c) 2014-2024 Zihao Yu, Nanjing University
 *
 * NEMU is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 *PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 *KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 *NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * See the Mulan PSL v2 for more details.
 ***************************************************************************************/

#include "common.h"
#include "debug.h"
#include <assert.h>
#include <elf.h>
#include <isa.h>
#include <memory/paddr.h>
#include <string.h>

void init_rand();
void init_log(const char *log_file);
void init_mem();
void init_difftest(char *ref_so_file, long img_size, int port);
void init_device();
void init_sdb();
void init_disasm();

static void welcome() {
  Log("Trace: %s", MUXDEF(CONFIG_TRACE, ANSI_FMT("ON", ANSI_FG_GREEN),
                          ANSI_FMT("OFF", ANSI_FG_RED)));
  IFDEF(CONFIG_TRACE,
        Log("If trace is enabled, a log file will be generated "
            "to record the trace. This may lead to a large log file. "
            "If it is not necessary, you can disable it in menuconfig"));
  Log("Build time: %s, %s", __TIME__, __DATE__);
  printf("Welcome to %s-NEMU!\n",
         ANSI_FMT(str(__GUEST_ISA__), ANSI_FG_YELLOW ANSI_BG_RED));
  printf("For help, type \"help\"\n");
}

#ifndef CONFIG_TARGET_AM
#include <getopt.h>

void sdb_set_batch_mode();

static char *log_file = NULL;
static char *diff_so_file = NULL;
static char *img_file = NULL;
static char *elf_file = NULL;
static int difftest_port = 1234;
static char *elf_buf = NULL;
int elf_flag = 0;
static long load_img() {
  if (img_file == NULL) {
    Log("No image is given. Use the default build-in image.");
    return 4096; // built-in image size
  }

  FILE *fp = fopen(img_file, "rb");
  Assert(fp, "Can not open '%s'", img_file);

  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);

  Log("The image is %s, size = %ld", img_file, size);

  fseek(fp, 0, SEEK_SET);
  int ret = fread(guest_to_host(RESET_VECTOR), size, 1, fp);
  assert(ret == 1);

  fclose(fp);
  return size;
}
char *load_elf() {
  if (elf_file == NULL) {
    Log("No image is given.");
    assert(0);
  }
  FILE *fp = fopen(elf_file, "rb");
  Assert(fp, "Can not open '%s'", elf_file);
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);
  Log("The elf is %s, size = %ld", elf_file, size);
  char *buf = (char *)malloc(size);
  fseek(fp, 0, SEEK_SET);
  int ret = fread(buf, size, 1, fp);
  assert(ret == 1);
  fclose(fp);
  return buf;
}
#define FUNC_NUM 128
typedef struct {
  bool valid;
  char name[32];
  vaddr_t begin;
  vaddr_t end;
} Func;
static Func funcs[FUNC_NUM];
static int nr_func = 0;
void init_funcs() {
  for (int i = 0; i < FUNC_NUM; i++) {
    funcs[i].valid = 0;
  }
  return;
}
void open_elf(){
  elf_flag = 1;
}
int parse_elf() {
  if (elf_flag == 0){
    return 0;
  }
  elf_buf = load_elf();
  init_funcs();
  Elf32_Ehdr *ehdr = (Elf32_Ehdr *)elf_buf;
  Elf32_Shdr *shdrs = (Elf32_Shdr *)(elf_buf + ehdr->e_shoff);
  Elf32_Shdr *shstr_shdr =
      &shdrs[ehdr->e_shstrndx]; //将section header部分解释成一个表
  const char *shstrtab = elf_buf + shstr_shdr->sh_offset;
  int sym_index = 0;
  int sym_link = 0;
  for (int i = 0; i < ehdr->e_shnum; i++) {
    const char *name = shstrtab + shdrs[i].sh_name;
    if (strcmp(name, ".symtab") == 0) {
      sym_index = i;
      sym_link = shdrs[i].sh_link;
    }
    Log("[%d] %s type=%u offset=0x%x size=0x%x link=%d\n", i, name,
        shdrs[i].sh_type, shdrs[i].sh_offset, shdrs[i].sh_size,
        shdrs[i].sh_link);
  }
  if (sym_index == 0) {
    Log("No .symtab found, ftrace disabled");
    assert(0);
    return 0;
  }
  
  Elf32_Shdr *symtab_sh = &shdrs[sym_index];
  Elf32_Sym *symtab =
      (Elf32_Sym *)(elf_buf + symtab_sh->sh_offset); //将这段内存解释为sym表
  int nr_sym = symtab_sh->sh_size / symtab_sh->sh_entsize;
  Elf32_Shdr *strtab_sh = &shdrs[sym_link];
  const char *strtab = elf_buf + strtab_sh->sh_offset;
 
  nr_func = 0;
  for (int i = 0; i < nr_sym; i++) {
    Elf32_Sym *s = &symtab[i];

    if (ELF32_ST_TYPE(s->st_info) != STT_FUNC)  //跳过非函数，0大小，大于FUNC_NUM
      continue;
    if (s->st_size == 0)
      continue;
    if (nr_func >= FUNC_NUM)
      break;

    const char *name = strtab + s->st_name;

    funcs[nr_func].valid = true;
    strncpy(funcs[nr_func].name, name, sizeof(funcs[nr_func].name) - 1);
    funcs[nr_func].name[sizeof(funcs[nr_func].name) - 1] = '\0';

    funcs[nr_func].begin = (vaddr_t)s->st_value;
    funcs[nr_func].end = funcs[nr_func].begin + s->st_size;

    nr_func++;
  }
  return nr_func;
}

static int parse_args(int argc, char *argv[]) {
  for (int i = 0; i < argc; i++) {
  Log("argv[%d] = %s", i, argv[i]);
} 
  const struct option table[] = {
      {"batch", no_argument, NULL, 'b'},
      {"log", required_argument, NULL, 'l'},
      {"diff", required_argument, NULL, 'd'},
      {"port", required_argument, NULL, 'p'},
      {"help", no_argument, NULL, 'h'},
      {"elf", required_argument, NULL, 'e'},
      {0, 0, NULL, 0},
  };
  int o;
  while ((o = getopt_long(argc, argv, "-bhl:d:p:", table, NULL)) != -1) {
    switch (o) {
    case 'b':
      sdb_set_batch_mode();
      break;
    case 'p':
      sscanf(optarg, "%d", &difftest_port);
      break;
    case 'l':
      log_file = optarg;
      break;
    case 'd':
      diff_so_file = optarg;
      break;
    case 'e':
      elf_file = optarg;
      open_elf();
      break;
    case 1:
      img_file = optarg;
      return 0;
    default:
      printf("Usage: %s [OPTION...] IMAGE [args]\n\n", argv[0]);
      printf("\t-b,--batch              run with batch mode\n");
      printf("\t-l,--log=FILE           output log to FILE\n");
      printf("\t-d,--diff=REF_SO        run DiffTest with reference REF_SO\n");
      printf("\t-p,--port=PORT          run DiffTest with port PORT\n");
      printf("\t-e,--elf=FILE          load a elf file\n");
      printf("\n");
      exit(0);
    }
  }
  return 0;
}

void init_monitor(int argc, char *argv[]) {
  /* Perform some global initialization. */

  /* Parse arguments. */
  parse_args(argc, argv);

  /* Set random seed. */
  init_rand();

  /* Open the log file. */
  init_log(log_file);

  /* Initialize memory. */
  init_mem();

  /* Initialize devices. */
  IFDEF(CONFIG_DEVICE, init_device());

  /* Perform ISA dependent initialization. */
  init_isa();

  /* Load the image to memory. This will overwrite the built-in image. */
  long img_size = load_img();

  /* Initialize differential testing. */
  init_difftest(diff_so_file, img_size, difftest_port);

  /*解析elf*/
  parse_elf();

  /* Initialize the simple debugger. */
  init_sdb();

  IFDEF(CONFIG_ITRACE, init_disasm());

  /* Display welcome message. */
  welcome();
}
#else // CONFIG_TARGET_AM
static long load_img() {
  extern char bin_start, bin_end;
  size_t size = &bin_end - &bin_start;
  Log("img size = %ld", size);
  memcpy(guest_to_host(RESET_VECTOR), &bin_start, size);
  return size;
}

void am_init_monitor() {
  Log("AM");
  init_rand();
  init_mem();
  init_isa();
  load_img();
  IFDEF(CONFIG_DEVICE, init_device());
  welcome();
}
#endif
