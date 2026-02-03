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
#include "common.h"
#include "sdb.h"
#include "trace/ftrace.h"
#include "trace/itrace.h"
#include "trace/mtrace.h"
#include "tui.h"
static int is_batch_mode = false;
void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
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
static int cmd_help(char *args);
static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}
// 问题1：printf是行缓冲，在接收到\n之后才会输出
// 单步执行
static int cmd_si(char * args){
  if(args == NULL){
    cpu_exec(1);
    return 0;
  }
  int n = atoi(args);
  cpu_exec(n);
  return 0;
}
//退出
static int cmd_q(char *args) {
  nemu_state.state = NEMU_END;
  return -1;
}
//表达式求值
static int cmd_p(char * args){
  char * expr_str = args;
  bool success = true;
  int result = expr(expr_str,&success);
  printf("%d\n",result);
  return 0;
}
static int expr_test(char * args){
  char * expr_str = args;
  bool success = true;
  unsigned int result = expr(expr_str,&success);
  fprintf(stderr,"%u %s\n",result,expr_str);
  return 0;
}
static int cmd_ph(char*args){
  char * expr_str = args;
  bool success = true;
  int result = expr(expr_str,&success);
  printf("0x%08x\n",result);
  return 0;
}

static int cmd_x(char * args){
  //"x n addr"
  char *n_str  = strtok(args, " ");
  char *addr_str  = strtok(NULL, " ");
  int n = atoi(n_str);
  paddr_t addr = strtol(addr_str, NULL, 0);
  word_t buffer[n];
  for (int i = 0;i<n;i++){
    buffer[i] = paddr_read(addr+i*4,4);
  }
  for (int i = 0;i<n;i++){
    printf("0x%08x\n",buffer[i]);
  }
  return 0;
}
static int cmd_info(char*args){
  if (*args == 'r'){
    isa_reg_display();
    return 0;
  }else{
    current_wp();
    return 0;
  }
}
static int cmd_w(char * args){
  set_wp(args);
  return 0;
}

static int cmd_d(char*args){
  int n = atoi(args);
  delete_wp(n);
  return 0;
}
static int cmd_m(char*args){
  dump_mtrace();
  return 0;
}
static int cmd_help(char *args);
static int cmd_f(char*args){
  ftrace_print();
  return 0;
}
static int cmd_i(char*args){
  print_recent_insts();
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
    }

    va_end(ap);
    return n;
}

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "p" , "calculate expr",cmd_p},
  { "ph", "calculate expr with hex output",cmd_ph},
  { "si", "execute n instructions",cmd_si},
  { "x", "x N EXPR", cmd_x },
  { "info","info r to show regs\n info w to show watchpoints",cmd_info},
  { "w", "w EXPR to set a watchpoint",cmd_w},
  { "d", "delete number N watchpoint",cmd_d},
  { "t", "to test expr",expr_test},
  { "f", "show function tree",cmd_f},
  { "m", "show mtrace",cmd_m},
  {"i","print recent insts",cmd_i},
  {"layout","layout asm,src,split",cmd_layout}
  /* TODO: Add more commands */

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
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
  if (args == NULL) {
    args = ""; 
  }
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
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
