#include "device.h"
#include "am.h"
#include <stdio.h>
#if defined(MULTIPROGRAM) && !defined(TIME_SHARING)
# define MULTIPROGRAM_YIELD() yield()
#else
# define MULTIPROGRAM_YIELD()
#endif

#define NAME(key) \
  [AM_KEY_##key] = #key,

static const char *keyname[256] __attribute__((used)) = {
  [AM_KEY_NONE] = "NONE",
  AM_KEYS(NAME)
};

size_t serial_write(const void *buf, size_t offset, size_t len) {
  const char *str = (const char *)buf;
  for (size_t i = 0; i < len; i++) {
    putch(str[i]); 
  }
  return len;
}

size_t events_read(void *buf, size_t offset, size_t len) {
  AM_INPUT_KEYBRD_T ev = io_read(AM_INPUT_KEYBRD);
  if (ev.keycode == AM_KEY_NONE) {
    return snprintf(buf, len, "NONE\n");
  }
  Log("Key event detected: keycode=%d, down=%d", ev.keycode, ev.keydown);
  int ret = snprintf(buf, len, "%s %s\n", 
                     ev.keydown ? "kd" : "ku", 
                     keyname[ev.keycode]);
  return ret;
}
size_t dispinfo_read(void *buf, size_t offset, size_t len) {
  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
  int ret = snprintf(buf, len, "WIDTH:%d\nHEIGHT:%d\n", cfg.width, cfg.height);
  return ret;
}

size_t fb_write(const void *buf, size_t offset, size_t len) {
  // 规定len==0为同步信号
  if (len == 0) {
    io_write(AM_GPU_FBDRAW, 0, 0, NULL, 0, 0, true);
    return 0;
  }

  // 获取屏幕宽度用于解析 offset
  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
  int w = cfg.width;

  // 收到 4 个整数，说明是来自 NDL_DrawRect 的同步请求
  if (len == sizeof(int) * 4) { 
    int *a = (int *)buf;
    // 打印同步请求的起始坐标 (a[0], a[1]) 和区域大小 (a[2], a[3])
    printf("[fb_sync] x = %d, y = %d, w = %d, h = %d\n", a[0], a[1], a[2], a[3]);
    io_write(AM_GPU_FBDRAW, a[0], a[1], NULL, a[2], a[3], true);
    return len;
  }

  // 否则是传统的直接写入模式
  int p_idx = offset / 4; 
  int x = p_idx % w;
  int y = p_idx / w;

  // 打印解析 offset 得到的坐标
  printf("[fb_write] offset = %d, x = %d, y = %d, len = %d\n", (int)offset, x, y, (int)len);
  
  io_write(AM_GPU_FBDRAW, x, y, (void *)buf, len / 4, 1, false);
  return len;
}

void init_device() {
  Log("Initializing devices...");
  ioe_init();
}
