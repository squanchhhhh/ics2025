#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/time.h>
static int evtdev = -1;
static int fbdev = -1;
static int screen_w = 0, screen_h = 0;
static uint32_t boot_time = 0;
static int events_fd = -1;
static int canvas_h = 0,canvas_w = 0;

uint32_t NDL_GetTicks() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  uint32_t ms = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
  return ms - boot_time;
}


int NDL_PollEvent(char *buf, int len) {
  if (events_fd == -1) return 0; 

  int byte_read = read(events_fd, buf, len);
  
  return (byte_read > 0); 
}

void NDL_OpenCanvas(int *w, int *h) {
  if (getenv("NWM_APP")) {
    int fbctl = 4;
    fbdev = 5;
    screen_w = *w; screen_h = *h;
    char buf[64];
    int len = sprintf(buf, "%d %d", screen_w, screen_h);
    // let NWM resize the window and create the frame buffer
    write(fbctl, buf, len);
    while (1) {
      // 3 = evtdev
      int nread = read(3, buf, sizeof(buf) - 1);
      if (nread <= 0) continue;
      buf[nread] = '\0';
      if (strcmp(buf, "mmap ok") == 0) break;
    }
    close(fbctl);
  }
  char buf[64];
  int fd = open("/proc/dispinfo", 0, 0);
  read(fd, buf, sizeof(buf));
  
  // 解析 WIDTH 和 HEIGHT
  sscanf(buf, "WIDTH:%d\nHEIGHT:%d", &screen_w, &screen_h);
  close(fd);

  // 如果传入的 *w 或 *h 为 0，则设为屏幕大小
  if (*w == 0) *w = screen_w;
  if (*h == 0) *h = screen_h;
  
  // 记录画布大小
  canvas_w = *w; canvas_h = *h;
  printf("NDL: Screen size %d x %d, Canvas size %d x %d\n", screen_w, screen_h, *w, *h);
}

void NDL_DrawRect(uint32_t *pixels, int x, int y, int w, int h) {
  int fb = open("/dev/fb", 0, 0);
  int canvas_x0 = (screen_w - canvas_w) / 2;
  int canvas_y0 = (screen_h - canvas_h) / 2;

  for (int i = 0; i < h; i++) {
    uint32_t offset = ((canvas_y0 + y + i) * screen_w + (canvas_x0 + x)) * 4;
    lseek(fb, offset, SEEK_SET);
    write(fb, pixels + i * w, w * 4);
  }
  close(fb);
}

void NDL_OpenAudio(int freq, int channels, int samples) {
}

void NDL_CloseAudio() {
}

int NDL_PlayAudio(void *buf, int len) {
  return 0;
}

int NDL_QueryAudio() {
  return 0;
}

int NDL_Init(uint32_t flags) {
  if (getenv("NWM_APP")) {
    evtdev = 3;
  }
  struct timeval tv;
  gettimeofday(&tv, NULL);
  boot_time = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);

  events_fd = open("/dev/events", 0);
  
  return 0;
}
void NDL_Quit() {
}
