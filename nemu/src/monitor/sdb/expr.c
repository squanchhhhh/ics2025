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

#include "debug.h"
#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256,
  TK_EQ,        // ==
  TK_AND,       // &&
  TK_NOT,       // !=
  TK_STAR,      // *
  TK_DIV,       // /
  TK_PLUS,      // +
  TK_MINUS,     // -
  TK_NUM,       //
  TK_ADDR,      // &addr
  TK_REG,       // 
  TK_LEFT,      // (
  TK_RIGHT,     // )
  TK_HEX,
  TK_END
};
static struct rule {
  const char *regex;
  int token_type;
} rules[] = {
  { " +"       , TK_NOTYPE },   // spaces
  { "0[xX][0-9a-fA-F]+u?",TK_HEX},
  { "=="       , TK_EQ     },   // equal
  { "&&"       , TK_AND    },
  { "!="       , TK_NOT    },
  { "\\*"      , TK_STAR   },
  { "/"        , TK_DIV    },
  { "\\+"      , TK_PLUS   },   // plus
  { "-"        , TK_MINUS  },
  { "[0-9]+u?"   , TK_NUM    },
  { "&"        , TK_ADDR   },
  {"\\$(0|ra|sp|gp|tp|pc)", TK_REG},
  {"\\$(t[0-6]|s[0-9]|s1[01]|a[0-7])", TK_REG},
  { "\\("      , TK_LEFT   },
  { "\\)"      , TK_RIGHT  },

};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[256] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;
  nr_token = 0;
  while (e[position] != '\0') {
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;
        //排除空格
        if(i!=0){
          strncpy(tokens[nr_token].str, substr_start, substr_len);
          tokens[nr_token].str[substr_len] = '\0';
          tokens[nr_token].type = rules[i].token_type;
          nr_token++;}
        break;
      }
    }
    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }
  if(nr_token<256){
    tokens[nr_token].type = TK_END;
  }else{
    printf("nr_token = %d > 256",nr_token);
    assert(0);
  }
  return true;
}
int parse_error = 0;
typedef struct Parser Parser;
struct Parser{
    int pos;
    Token (*peek)(Parser * self);
    Token (*consume)(Parser * self);
    int (*parse_expr)(Parser * self);
    int (*parse_and)(Parser * self);
    int (*parse_eq_not)(Parser * self);
    int (*parse_plus_minus)(Parser * self);
    int (*parse_star_div)(Parser * self);
    int (*parse_unary)(Parser * self);
    int (*parse_primary)(Parser * self);   // 忘记了为什么之前会改成long
};
Token peek(Parser * self){
    return tokens[self->pos];
}
Token consume(Parser * self){
    return tokens[self->pos++];
}
int parse_expr(Parser * self){
    int val = self->parse_and(self);
    return val;
}
int parse_and(Parser * self){
    int value = self->parse_eq_not(self);
    while(self->peek(self).type == TK_AND){
        self->consume(self);
        int right = self->parse_eq_not(self);
        value = right && value;
    }
    return value;
}
int parse_eq_not(Parser* self){
    int value = self->parse_plus_minus(self);
    while (self->peek(self).type == TK_EQ || self->peek(self).type==TK_NOT){
        if (self->peek(self).type == TK_EQ){
            self->consume(self);
            int right = self->parse_plus_minus(self);
            value = value == right;
        }else if(self->peek(self).type == TK_NOT){
            self->consume(self);
            int right = self->parse_plus_minus(self);
            value = value != right;
        }
    }
    return value;
}
int parse_plus_minus(Parser *self) {
    int value = self->parse_star_div(self);
    while (self->peek(self).type == TK_PLUS || self->peek(self).type == TK_MINUS) {
        if (self->peek(self).type == TK_PLUS) {
            self->consume(self);
            int right = self->parse_star_div(self);
            value = value + right;
        } else if (self->peek(self).type == TK_MINUS) {
            self->consume(self);
            int right = self->parse_star_div(self);
            value = value - right;
        }
    }
    return value;
}
int parse_star_div(Parser *self) {
    int value = self->parse_unary(self);
    while (self->peek(self).type == TK_STAR || self->peek(self).type == TK_DIV) {
        if (self->peek(self).type == TK_STAR) {
            self->consume(self);
            int right = self->parse_unary(self);
            value = value * right;
        } else if (self->peek(self).type == TK_DIV) {
            self->consume(self);
            int right = self->parse_unary(self);
            assert(right != 0);
            value = value / right;
        }
    }
    return value;
}
int parse_unary(Parser *self) {
    if (self->peek(self).type == TK_MINUS) {
        self->consume(self);
        int val = self->parse_unary(self);
        return -val;
    }
    if (self->peek(self).type == TK_ADDR){
        self->consume(self);
        int val = self->parse_unary(self);
        return val;
    }
    if(self->peek(self).type == TK_STAR){
        self->consume(self);
        int addr = self->parse_unary(self);
        int value = paddr_read((paddr_t)addr,4);
        return value;
    }
    return self->parse_primary(self);
}
int parse_primary(Parser *self) {
    Token tk = self->peek(self);
    if (tk.type == TK_NUM) {
        self->consume(self);
        return atoi(tk.str);
    }
    if (tk.type == TK_HEX){
      self->consume(self);
      return strtol(tk.str,NULL,16);
    }
    if(tk.type == TK_REG){
        self->consume(self);
        bool success = true;
        int value;
        if(strcmp(tk.str,"pc")){
           value = cpu.pc;
        }
        else{value = isa_reg_str2val(tk.str,&success);}
        return value;
    }
    if (tk.type == TK_LEFT) {
        self->consume(self);
        int val = self->parse_expr(self);
        if (self->peek(self).type != TK_RIGHT) {
            printf("Missing TK_RIGHT\n");
        } else {
            self->consume(self);
        }
        return val;
    }
    assert(0);
    return 0;
}
Parser * init_parser() {
    Parser *parser = (Parser *)malloc(sizeof(Parser));
    parser->pos = 0;
    parser->peek = peek;
    parser->consume = consume;
    parser->parse_expr = parse_expr;
    parser->parse_and = parse_and;
    parser->parse_eq_not = parse_eq_not;
    parser->parse_plus_minus = parse_plus_minus;
    parser->parse_star_div = parse_star_div;
    parser->parse_unary = parse_unary;
    parser->parse_primary = parse_primary;
    return parser;
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }
  Parser * parser = init_parser();
  long result = parser->parse_expr(parser);
  return result;
}
