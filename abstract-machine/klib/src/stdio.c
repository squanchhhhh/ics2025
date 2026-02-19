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

int itox(char *buf, uint32_t x) {
  char tmp[16];
  char hex_map[] = "0123456789abcdef";
  int i = 0;

  if (x == 0) {
    buf[0] = '0';
    return 1;
  }

  while (x > 0) {
    tmp[i++] = hex_map[x % 16];
    x /= 16;
  }

  int len = 0;
  while (i > 0) {
    buf[len++] = tmp[--i];
  }
  return len;
}

//对于带n的输出，需要记录字符流的去向以及写入的字符数
typedef struct {
  char *out;
  size_t n;
  size_t written; // 实际写入内存的字符数
} SnprintfCtx;

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
void uart_handler(char c, void *ctx) { putch(c); }

// 辅助函数：处理对齐和填充
static int print_padded(putc_handler_t handler, void *ctx, const char *content, 
                        int len, int width, bool left_align, char pad_char) {
    int count = 0;
    int pad_len = (width > len) ? (width - len) : 0;

    if (!left_align) {
        for (int i = 0; i < pad_len; i++) { handler(pad_char, ctx); count++; }
    }
    for (int i = 0; i < len; i++) { handler(content[i], ctx); count++; }
    if (left_align) {
        for (int i = 0; i < pad_len; i++) { handler(' ', ctx); count++; } 
    }
    return count;
}

int vfmt_print(putc_handler_t handler, void *ctx, const char *fmt, va_list ap) {
  int char_count = 0;
  while (*fmt) {
    if (*fmt != '%') {
      handler(*fmt++, ctx); char_count++;
      continue;
    }

    fmt++; // 跳过 '%'
    bool left_align = false;
    char pad_char = ' ';
    int width = 0;

    if (*fmt == '-') { left_align = true; fmt++; }
    if (*fmt == '0') { pad_char = '0'; fmt++; }
    while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt++ - '0'); }

    char buf[64]; 
    char *s = buf;
    int len = 0;

    switch (*fmt) {
      case 's':
        s = va_arg(ap, char *);
        if (!s) s = "(null)";
        len = strlen(s);
        break;
      case 'd':
        len = itoa(buf, va_arg(ap, int));
        break;
      case 'x':
        len = itox(buf, va_arg(ap, uint32_t));
        break;
      case 'p':
        handler('0', ctx); handler('x', ctx); char_count += 2;
        len = itox(buf, (uintptr_t)va_arg(ap, void *));
        width = (width > 2) ? width - 2 : 0; // 减去 0x 的宽度
        break;
      case 'c':
        buf[0] = (char)va_arg(ap, int);
        len = 1;
        break;
      default: // 处理 %% 或者未知格式
        buf[0] = *fmt;
        len = 1;
    }
    
    char_count += print_padded(handler, ctx, s, len, width, left_align, pad_char);
    fmt++;
  }
  return char_count;
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