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

#include "sdb.h"
#include <stdio.h>
#include <isa.h>
#define NR_WP 32

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;
  char expr[128];   
  word_t last_val;

} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}
WP* new_wp() {
  if (free_ == NULL) {
    assert(0);
  }
  WP *wp = free_;
  free_ = free_->next;
  wp->next = head;
  head = wp;
  return wp;
}
void free_wp(WP *wp) {
  if (wp == NULL) return;
  if (head == wp) {
    head = wp->next;
  } else {
    WP *p = head;
    while (p != NULL && p->next != wp) {
      p = p->next;
    }
    p->next = wp->next;
  }
  wp->next = free_;
  free_ = wp;
}
void set_wp(char *expr_str) {
  WP *wp = new_wp();
  strncpy(wp->expr, expr_str, sizeof(wp->expr) - 1);
  wp->expr[sizeof(wp->expr) - 1] = '\0';
  bool  success = true;
  wp->last_val = expr(wp->expr,&success); 
  assert(success);
}
bool check_wp(WP *wp) {
  bool  success = true;
  word_t new_val = expr(wp->expr,&success);
  assert(success);
  if (new_val != wp->last_val) {
    printf("Watchpoint %d triggered at pc = 0x%x\n", wp->NO, cpu.pc);
    printf("  expr = %s\n", wp->expr);
    printf("  old value = 0x%x\n", wp->last_val);
    printf("  new value = 0x%x\n", new_val);
    wp->last_val = new_val;
    return true;
  }
  return false;
}
void current_wp(){
  WP*temp = head;
  while(temp!=NULL){
    printf("NO: %d  expr: %s  value: 0x%x\n",
       temp->NO, temp->expr, temp->last_val);
    temp = temp->next;
  }
  return ;
}
bool check_all_wp() {
  WP *wp = head;
  while (wp != NULL) {
    if (check_wp(wp)) {
      return true; 
    }
    wp = wp->next;
  }
  return false;
}
void delete_wp(int n) {
    WP *prev = NULL;
    WP *cur = head;
    while (cur != NULL && cur->NO != n) {
        prev = cur;
        cur = cur->next;
    }
    if (cur == NULL) {
        return;
    }
    if (prev == NULL) {
        head = cur->next;
    } else {
        prev->next = cur->next;
    }
    cur->next = free_;
    free_ = cur;
    printf("deleted wp number %d\n",n);
}
/* TODO: Implement the functionality of watchpoint */

