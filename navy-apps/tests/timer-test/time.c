#include <stdio.h>
#include <sys/time.h>

int main() {
  struct timeval tv;
  uint32_t last_ms = 0;

  printf("Timer test started!\n");

  while (1) {
    // 1. 调用系统调用
    gettimeofday(&tv, NULL);

    // 2. 转换为毫秒 (1s = 1000ms, 1000us = 1ms)
    uint32_t current_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    // 3. 判断是否过了 500 毫秒
    if (current_ms - last_ms >= 500) {
      printf("0.5 seconds passed. Current time: %u ms\n", current_ms);
      last_ms = current_ms;
    }
  }
  return 0;
}