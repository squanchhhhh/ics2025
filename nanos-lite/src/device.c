#include "device.h"
#include "am.h"
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

    return 0;
  }
  //Log("Key event detected: keycode=%d, down=%d", ev.keycode, ev.keydown);
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
/*
size_t fb_write(const void *buf, size_t offset, size_t len) {
  if (buf == NULL && len > 0) {
    printf("Bug Found: fb_write received NULL pixels from userland!\n");
} 
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
}*/
size_t fb_write(const void *buf, size_t offset, size_t len) {
  // 1. 严格检查 buf，如果地址太低（比如小于 0x1000），极有可能是无效指针
  if (buf == NULL && len > 0) {
    printf("[fb_write] ERROR: pixels buffer is NULL!\n");
    return 0;
  }

  // 2. 获取屏幕宽度用于解析 offset
  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
  int w = cfg.width;

  // 3. 处理同步信号 (len == 0)
  if (len == 0) {
    // 根据你的环境，这里可能需要传结构体，也可能是多参数
    // 我们先按你之前能跑通的多参数格式写
    io_write(AM_GPU_FBDRAW, 0, 0, NULL, 0, 0, true);
    return 0;
  }

  // 4. 处理 NDL_DrawRect 的同步请求 (4个int)
  if (len == sizeof(int) * 4) { 
    int *a = (int *)buf;
    printf("[fb_sync] x = %d, y = %d, w = %d, h = %d, buf_ptr = %p\n", a[0], a[1], a[2], a[3], buf);
    io_write(AM_GPU_FBDRAW, a[0], a[1], NULL, a[2], a[3], true);
    return len;
  }

  // 5. 传统的直接写入模式：解析坐标
  int p_idx = offset / 4; 
  int x = p_idx % w;
  int y = p_idx / w;

  // --- 关键 Log：对比 mtrace ---
  // 如果这里打印的 buf 是 0x83xxxxxx，但后面崩溃报 0x0，
  // 证明 io_write 内部传参发生了偏移。
  printf("[fb_write] x=%d, y=%d, buf=%p, len=%d\n", x, y, buf, (int)len);

  // 6. 绘图调用
  // 请注意：如果你的 io_write 定义是多参数的，确保参数顺序完全一致
  io_write(AM_GPU_FBDRAW, x, y, (void *)buf, len / 4, 1, false);

  return len;
}
void init_device() {
  Log("Initializing devices...");
  ioe_init();
}
