#include <am.h>
#include <nemu.h>

#define SYNC_ADDR (VGACTL_ADDR + 4)
static int screen_w = 0;
static int screen_h = 0;
void __am_gpu_init() {
  uint32_t wh = inl(VGACTL_ADDR);
  screen_w = wh & 0xffff;
  screen_h = (wh >> 16) & 0xffff;
}

void __am_gpu_config(AM_GPU_CONFIG_T *cfg) {
  if (screen_w == 0 || screen_h == 0) {
    uint32_t wh = inl(VGACTL_ADDR);
    screen_w = wh & 0xffff;
    screen_h = (wh >> 16) & 0xffff;
  }
  *cfg = (AM_GPU_CONFIG_T) {
    .present  = true,
    .has_accel = false,
    .width    = screen_w,
    .height   = screen_h,
    .vmemsz   = screen_w * screen_h * 4
  };
}


void __am_gpu_fbdraw(AM_GPU_FBDRAW_T *ctl) {
  uint32_t *fb = (uint32_t *)(uintptr_t)FB_ADDR;
  uint32_t *pixels = ctl->pixels;

  int x = ctl->x;
  int y = ctl->y;
  int w = ctl->w;
  int h = ctl->h;

  for (int i = 0; i < h; i ++) {
    int fb_row = (y + i) * screen_w + x;
    int px_row = i * w;
    for (int j = 0; j < w; j ++) {
      fb[fb_row + j] = pixels[px_row + j];
    }
  }

  if (ctl->sync) {
    outl(SYNC_ADDR, 1);
  }
}

void __am_gpu_status(AM_GPU_STATUS_T *status) {
  status->ready = true;
}
