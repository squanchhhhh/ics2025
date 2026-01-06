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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// this should be enough
static char buf[65536] = {};
static int expr_index = 0;
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";


int choose(int n){
  return rand()%n;
}
typedef enum {
  NODE_NUM,
  NODE_OP
} NodeType;
typedef struct ASTNode {
  NodeType type;
  union {
    int value;             
    struct {
      char op;             
      struct ASTNode *left;
      struct ASTNode *right;
    };
  };
} ASTNode;
void mid_travel(ASTNode *root);
ASTNode *new_num_node(int val) {
  ASTNode *n = malloc(sizeof(ASTNode));
  n->type = NODE_NUM;
  n->value = val;
  return n;
}

ASTNode *new_op_node(char op, ASTNode *l, ASTNode *r) {
  ASTNode *n = malloc(sizeof(ASTNode));
  n->type = NODE_OP;
  n->op = op;
  n->left = l;
  n->right = r;
  return n;
}
//num最长为11，op长度为1，所有叶子节点在同一层，深度为 65536>11*2**n+2^n
ASTNode *gen_rand_ast(int depth) {
  if (depth <= 0 || choose(3) == 0) {
    return new_num_node(rand() % 1000);
  }

  ASTNode *left = gen_rand_ast(depth - 1);
  ASTNode *right = gen_rand_ast(depth - 1);
  char ops[] = "+-*";
  char op = ops[rand() % 3];

  return new_op_node(op, left, right);
}
ASTNode *gen_div_node(int depth) {
  ASTNode *l = gen_rand_ast(depth - 1);
  ASTNode *r;
  do {
    r = new_num_node(rand()%1000);
  } while (r->value == 0);
  return new_op_node('/', l, r);
}

void mid_travel(ASTNode *root) {
  if (root == NULL) return;
  int need_paren = (root->type == NODE_OP) && (rand() % 2);

  if (need_paren) {
    buf[expr_index++] = '(';
  }
  mid_travel(root->left);

  char temp[32];
  size_t written = 0;

  if (root->type == NODE_NUM) {
    char * format = rand()%2?"%uu":"0x%xu";
    int len = snprintf(temp, sizeof(temp), format, root->value);
    written = (len < sizeof(temp)) ? len : sizeof(temp) - 1;
  } 
  else if (root->type == NODE_OP) {
    temp[0] = ' ';
    temp[1] = root->op;
    temp[2] = ' ';
    temp[3] = '\0';
    written = 3;
  }
  memcpy(buf + expr_index, temp, written);
  expr_index += written;
  mid_travel(root->right);
  if (need_paren) {
    buf[expr_index++] = ')';
  }
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
    mid_travel(gen_rand_ast(6));

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result;
    ret = fscanf(fp, "%d", &result);
    pclose(fp);

    printf("%u %s\n", result, buf);
  }
  return 0;
}
