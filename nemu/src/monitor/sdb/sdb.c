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
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "sdb.h"
#include "trace/ftrace.h"
#include "trace/itrace.h"
#include "trace/mtrace.h"
#include "trace/tui.h"

/* --- 函数原型声明 (Function Prototypes) --- */

// 从标准输入读取一行字符串，支持 readline 历史记录功能
static char* rl_gets();

// 打印帮助信息，列出所有支持的 SDB 命令及其描述
static int cmd_help(char *args);

// 继续执行程序，直到遇到断点、监控点或程序结束
static int cmd_c(char *args);

// 单步执行 n 条指令，如果不带参数则默认执行 1 条
static int cmd_si(char *args);

// 退出 NEMU 调试器，将状态设为 NEMU_END
static int cmd_q(char *args);

// 对给定的表达式进行求值并以十进制格式打印结果
static int cmd_p(char *args);

// 用于测试表达式求值系统的正确性，打印求值结果和原始字符串
static int expr_test(char *args);

// 对表达式进行求值并以十六进制（Hex）格式打印结果
static int cmd_ph(char *args);

// 扫描内存（examine），从指定地址开始以十六进制打印 n 个字
static int cmd_x(char *args);

// 显示调试相关信息：'info r' 显示寄存器，'info w' 显示监控点
static int cmd_info(char *args);

// 在指定的表达式上设置一个新的监控点（Watchpoint）
static int cmd_w(char *args);

static int cmd_b(char *args);

// 根据监控点的序号删除指定的监控点
static int cmd_d(char *args);

// 打印内存访问追踪（mtrace）的记录，用于调试内存读写行为
static int cmd_m(char *args);

// 打印函数调用追踪（ftrace），显示程序的函数调用层级
static int cmd_f(char *args);

// 打印最近执行过的指令列表（itrace），方便定位死循环或崩溃位置
static int cmd_i(char *args);
static int cmd_v(char *args);
static int cmd_bt(char *args);
// 封装后的 printf，支持在 TUI 模式下将输出重定向到专用窗口
int nemu_printf(const char *fmt, ...);

// 外部初始化函数，定义在其他源文件中
void init_regex();
void init_wp_pool();

/* --- 变量定义与命令表 --- */

static int is_batch_mode = false;

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "p" , "Calculate expression and print in decimal", cmd_p },
  { "ph", "Calculate expression and print in hex", cmd_ph },
  { "si", "Step execute N instructions", cmd_si },
  { "x", "Scan memory: x N EXPR", cmd_x },
  { "info","Show registers (r) or watchpoints (w)", cmd_info },
  { "w", "Set a watchpoint on expression: w EXPR", cmd_w },
  { "d", "Delete watchpoint by number: d N", cmd_d },
  { "t", "Test expression evaluation", expr_test },
  { "f", "Show function call tree (ftrace)", cmd_f },
  { "m", "Show memory access trace (mtrace)", cmd_m },
  { "i", "Print recently executed instructions", cmd_i },
  { "layout", "Switch TUI layout (asm, src, split)", cmd_layout },
  {"bt","print func frame",cmd_bt},
  {"b", "breakpoint",cmd_b},
  {"v","v_addr",cmd_v}
};

#define NR_CMD ARRLEN(cmd_table)

/* --- 函数具体实现 --- */

static char* rl_gets() {
  static char *line_read = NULL;
  if (line_read) {
    free(line_read);
    line_read = NULL;
  }
  line_read = readline("(nemu) ");
  if (line_read && *line_read) {
    add_history(line_read);
  }
  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_si(char * args){
  if (args == NULL || *args == '\0') {
    cpu_exec(1);
    return 0;
  }
  int n = atoi(args);
  cpu_exec(n);
  return 0;
}

static int cmd_q(char *args) {
  nemu_state.state = NEMU_END;
  return -1;
}

static int cmd_p(char * args){
  bool success = true;
  int result = expr(args, &success);
  if (success) printf("%d\n", result);
  return 0;
}

static int expr_test(char * args){
  bool success = true;
  unsigned int result = expr(args, &success);
  fprintf(stderr, "%u %s\n", result, args);
  return 0;
}

static int cmd_ph(char*args){
  bool success = true;
  int result = expr(args, &success);
  if (success) printf("0x%08x\n", result);
  return 0;
}

static int cmd_x(char * args){
  char *n_str  = strtok(args, " ");
  char *addr_str  = strtok(NULL, " ");
  if (n_str == NULL || addr_str == NULL) return 0;
  
  int n = atoi(n_str);
  paddr_t addr = strtol(addr_str, NULL, 0);
  for (int i = 0; i < n; i++){
    printf("0x%08x: 0x%08x\n", addr + i * 4, paddr_read(addr + i * 4, 4));
  }
  return 0;
}

static int cmd_v(char *args) {
  char *n_str = strtok(args, " ");
  char *vaddr_str = strtok(NULL, " ");

  if (n_str == NULL || vaddr_str == NULL) {
    printf("Usage: v [N] [VADDR]\n");
    return 0;
  }

  int n = atoi(n_str);
  vaddr_t vaddr = strtol(vaddr_str, NULL, 0);

  printf("Virtual Memory Scan at 0x%08x:\n", vaddr);
  for (int i = 0; i < n; i++) {
    word_t val = vaddr_read(vaddr + i * 4, 4);
    printf("v[0x%08x]:  p[0x%08x]  ->  0x%08x\n", 
            vaddr + i * 4, 
            isa_mmu_translate(vaddr + i * 4, 0, 0), 
            val);
  }
  return 0;
}

static int cmd_info(char*args){
  if (args != NULL && *args == 'r'){
    isa_reg_display();
  } else {
    current_wp();
  }
  return 0;
}

static int cmd_w(char * args){
  add_watchpoint(args);
  return 0;
}

static int cmd_d(char*args){
  delete_wp(atoi(args));
  return 0;
}

static int cmd_m(char*args){
  dump_mtrace();
  return 0;
}

static int cmd_f(char*args){
  dump_ftrace();
  return 0;
}

static int cmd_i(char*args){
  dump_insts();
  return 0;
}

static int cmd_help(char *args) {
  char *arg = strtok(NULL, " ");
  if (arg == NULL) {
    for (int i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  } else {
    for (int i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

int nemu_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = 0;
    if (is_tui_mode && tui_win != NULL) {
        vw_printw(tui_win, fmt, ap);
        wrefresh(tui_win);
    } else {
        n = vprintf(fmt, ap);
        fflush(stdout);
    }
    va_end(ap);
    return n;
}
static int cmd_bt(char *args){
  dump_ftrace_stack(); 
  return 0;
}
static int cmd_b(char *args){
  if (args == NULL) {
    printf("Usage: b ADDR\n");
    return 0;
  }
  vaddr_t addr;
  addr = strtol(args, NULL, 0); 
  
  if (addr == 0 && strcmp(args, "0") != 0) {
    printf("Invalid address: %s\n", args);
    return 0;
  }

  add_breakpoint(addr);
  printf("Breakpoint set at 0x%08x\n", addr);
  return 0;
}
void sdb_set_batch_mode() {
  is_batch_mode = true;
}

int sdb_execute(char *str) {
  if (str == NULL) return 0;
  char *cmd = strtok(str, " ");
  if (cmd == NULL) return 0;
  char *args = strtok(NULL, "");
  if (args == NULL) args = ""; 
  
  for (int i = 0; i < NR_CMD; i++) {
    if (strcmp(cmd, cmd_table[i].name) == 0) {
      return cmd_table[i].handler(args);
    }
  }
  printf("Unknown command '%s'\n", cmd);
  return 0;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL); 
    return;
  }
  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_copy = strdup(str);
    if (sdb_execute(str_copy) < 0) {
      free(str_copy);
      break;
    }
    free(str_copy);
  }
}

void init_sdb() {
  init_regex();
  init_wp_pool();
}