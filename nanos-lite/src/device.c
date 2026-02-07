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
static int write_count = 0;
size_t fb_write(const void *buf, size_t offset, size_t len) {
  write_count++;
  //if (write_count % 100 == 0) printf("Write calls: %d\n", write_count);
  //TIME_START();
  // 规定len==0为同步信号
  if (len == 0) {
    io_write(AM_GPU_FBDRAW, 0, 0, NULL, 0, 0, true);
    return 0;
  }

  AM_GPU_CONFIG_T cfg = io_read(AM_GPU_CONFIG);
  int w = cfg.width;
  int p_idx = offset / 4; 
  int x = p_idx % w;
  int y = p_idx / w;

  io_write(AM_GPU_FBDRAW, x, y, (void *)buf, len / 4, 1, false);
  //TIME_END("fb_write");
  return len;
}
void init_device() {
  Log("Initializing devices...");
  ioe_init();
}
