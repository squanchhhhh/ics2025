#include <am.h>
#include <klib-macros.h>
#include <klib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)
typedef void (*putc_handler_t)(char c, void *ctx);
int itoa(char *buf, int x) {
  char tmp[16];
  int i = 0;
  int neg = 0;
  if (x == 0) {
    buf[0] = '0';
    return 1;
  }
  if (x < 0) {
    neg = 1;
    x = -x;
  }
  while (x > 0) {
    tmp[i++] = '0' + (x % 10);
    x /= 10;
  }
  int len = 0;
  if (neg)
    buf[len++] = '-';
  while (i > 0) {
    buf[len++] = tmp[--i];
  }
  return len;
}


typedef struct {
    void (*putc)(char c, void *ctx); 
    void *ctx;                       
} PrintHandler;
//对于带n的输出，需要记录字符流的去向以及写入的字符数
typedef struct {
  char *out;
  size_t n;
  size_t written; // 实际写入内存的字符数
} SnprintfCtx;

int vfmt_print(putc_handler_t handler, void *ctx, const char *fmt, va_list ap) {
  int char_count = 0;
  while (*fmt) {
    if (*fmt != '%') {
      handler(*fmt, ctx);
      char_count++;
    } else {
      fmt++;
      switch (*fmt) {
        case 's': {
          char *s = va_arg(ap, char *);
          if (!s) s = "(null)";
          while (*s) {
            handler(*s++, ctx);
            char_count++;
          }
          break;
        }
        case 'd': {
          int d = va_arg(ap, int);
          char buf[32];
          int len = itoa(buf, d); 
          for (int i = 0; i < len; i++) {
            handler(buf[i], ctx);
            char_count++;
          }
          break;
        }
        case 'c': {
          char c = (char)va_arg(ap, int);
          handler(c, ctx);
          char_count++;
          break;
        }
      }
    }
    fmt++;
  }
  return char_count;
}
void snprintf_handler(char c, void *ctx) {
  SnprintfCtx *s_ctx = (SnprintfCtx *)ctx;
  //每写入一个字符就对written进行++
  //需要留一个位置给'\0'
  if (s_ctx->written < s_ctx->n - 1) {
    s_ctx->out[s_ctx->written] = c;
  }
  //返回本来想要写入的长度
  s_ctx->written++;
}

//将字符流输出给uart
void uart_handler(char c, void *ctx) {
  putch(c); 
}

int printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int len = vfmt_print(uart_handler, NULL, fmt, ap);
  va_end(ap);
  return len;
}
int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  SnprintfCtx ctx = {out, n, 0};
  int total_len = vfmt_print(snprintf_handler, &ctx, fmt, ap);
  // 结束后补 \0
  if (n > 0) {
    size_t final_pos = (ctx.written < n) ? ctx.written : n - 1;
    out[final_pos] = '\0';
  }
  return total_len;
}
int vsprintf(char *out, const char *fmt, va_list ap) {
  return vsnprintf(out, (size_t)-1, fmt, ap);
}
int sprintf(char *out, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int len = vsprintf(out, fmt, ap);
  va_end(ap);
  return len;
}
int snprintf(char *out, size_t n, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int len = vsnprintf(out, n, fmt, ap);
  va_end(ap);
  return len;
}

#endif