#include <stdio.h>
#include <stdint.h>
#include <NDL.h>  // 包含 NDL 头文件

int main() {
  // 1. 初始化 NDL 库
  // 参数 0 表示默认配置，目前 NDL_Init 暂时用不到复杂的参数
  if (NDL_Init(0) != 0) {
    printf("NDL Init failed!\n");
    return -1;
  }

  uint32_t last_ms = 0;

  printf("Timer test (using NDL) started!\n");

  while (1) {
    // 2. 调用 NDL 提供的封装好的毫秒级计时函数
    uint32_t current_ms = NDL_GetTicks();

    // 3. 判断是否过去了 0.5 秒 (500 毫秒)
    if (current_ms - last_ms >= 500) {
      printf("0.5 seconds passed. Current time: %u ms\n", current_ms);
      last_ms = current_ms;
    }
  }

  // 虽然这里是死循环，但良好的习惯是在退出前调用这个
  NDL_Quit();
  return 0;
}