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
    return snprintf(buf, len, "NONE\n");
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
  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
  int screen_w = cfg.width;

  // 2. 处理同步信号 (NDL 中 len == 0 通常代表显存同步请求)
  if (len == 0) {
    printf("enter sync\n");
    AM_GPU_FBDRAW_T rect = {
      .x = 0, .y = 0, .w = 0, .h = 0, 
      .pixels = NULL, 
      .sync = true
    };
    ioe_write(AM_GPU_FBDRAW, &rect);
    printf("quit sync\n");
    return 0;
  }

  // 3. 处理 NDL_DrawRect 的封装协议 (4个int组成的同步块)
  if (len == sizeof(int) * 4) {
    int *a = (int *)buf;
    AM_GPU_FBDRAW_T rect = {
      .x = a[0], 
      .y = a[1], 
      .w = a[2], 
      .h = a[3], 
      .pixels = NULL, 
      .sync = true
    };
    // 注意：必须传 &rect 指针，否则内核会把坐标值当地址读
    ioe_write(AM_GPU_FBDRAW, &rect);
    return len;
  }

  // 4. 普通像素写入模式 (基于 offset 解析坐标)
  int p_idx = offset / 4; 
  int x = p_idx % screen_w;
  int y = p_idx / screen_w;

  // 构造绘图结构体
  AM_GPU_FBDRAW_T rect = {
    .x = x,
    .y = y,
    .w = len / 4, // 像素个数
    .h = 1,       // 这种模式下通常按行写入
    .pixels = (void *)buf,
    .sync = false
  };

  // 调用 AM 接口绘制一行
  ioe_write(AM_GPU_FBDRAW, &rect);

  return len;
}
void init_device() {
  Log("Initializing devices...");
  ioe_init();
}
