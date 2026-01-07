#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
  int len = 0;
  while(s[len]!='\0'){   //*s卡了20分钟
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
    while(*s1 && *s2) {
        int diff = (unsigned char)*s1 - (unsigned char)*s2;
        if(diff != 0) return diff;
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  int index = 0;
  while(s1[index] != '\0' && s2[index] != '\0' && index<n){
    int temp = s1[index] - s2[index];
    if(temp !=0){
      return temp;
    }else{
      index++;
    }
  }
  return s1[index] - s2[index];
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    unsigned char val = (unsigned char)c; 
    for (size_t i = 0; i < n; i++) {
        p[i] = val;
    }
    return s;
}

void *memmove(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    if (d == s || n == 0) return dst;
    if (d < s) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = n; i > 0; i--) {
            d[i-1] = s[i-1];
        }
    }
    return dst;
}

void *memcpy(void *out, const void *in, size_t n) {
    unsigned char *d = out;
    const unsigned char *s = in;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return out;
}
int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = s1;
    const unsigned char *p2 = s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i])
            return p1[i] - p2[i];
    }
    return 0;
}

#endif
