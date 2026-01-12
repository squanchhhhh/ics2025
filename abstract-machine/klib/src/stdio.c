#include <am.h>
#include <klib-macros.h>
#include <klib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)
int itoa(char *buf, int x);
int fmt_resolve(char * out,const char * fmt,va_list ap){
  int out_index = 0;
  while (*fmt) {
    if (*fmt != '%') {
      out[out_index++] = *fmt++;
      continue;
    }
    fmt++;
    switch (*fmt) {
    case 'd': {
      int x = va_arg(ap, int);
      int len = itoa(&out[out_index], x);
      out_index += len;
      break;
    }
    case 's': {
      char *s = va_arg(ap, char *);
      int len = strlen(s);
      memcpy(&out[out_index], s, len);
      out_index += len;
      break;
    }
    }
    fmt++;
  }
  out[out_index] = '\0';
  return out_index;
}
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

int printf(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  int len = fmt_resolve(buf, fmt, ap);
  va_end(ap);
  for (int i = 0;i<len;i++){
    putch(buf[i]);
  }
  return len;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  panic("Not implemented");
}

int sprintf(char *out, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int len = fmt_resolve(out,fmt,ap);
  va_end(ap);
  return len;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  panic("Not implemented");
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  panic("Not implemented");
}

#endif
