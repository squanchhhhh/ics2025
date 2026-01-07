#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
  int len = 0;
  while(*s!='\0'){
    len ++;
  }
  return len;
}

char *strcpy(char *dst, const char *src) {
  char * head = dst;
    while(*src!='\0'){
      *dst++ = *src++;
    }
    *dst = '\0';
    return head;
}

char *strncpy(char *dst, const char *src, size_t n) {
    char *head = dst;
    int i = 0;
    for (; i < n && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    for (; i < n; i++) {
        dst[i] = '\0';
    }
    return head;
}

char *strcat(char *dst, const char *src) {
  char * head = dst;
  int len = strlen(dst);
  int total = len+strlen(src);
  for (int i = len;i<total;i++){
    dst[i] = src[i-len];
  }
  dst[total]= '\0';
  return head;
}

int strcmp(const char *s1, const char *s2) {
  panic("Not implemented");
}

int strncmp(const char *s1, const char *s2, size_t n) {
  panic("Not implemented");
}

void *memset(void *s, int c, size_t n) {
  panic("Not implemented");
}

void *memmove(void *dst, const void *src, size_t n) {
  panic("Not implemented");
}

void *memcpy(void *out, const void *in, size_t n) {
  panic("Not implemented");
}

int memcmp(const void *s1, const void *s2, size_t n) {
  panic("Not implemented");
}

#endif
