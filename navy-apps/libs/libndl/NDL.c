#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/time.h>

#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define MAP_SHARED 0x01

extern void* mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
extern int open(const char *pathname, int flags, ...);


static int evtdev = -1;
static int fbdev = -1;
static int screen_w = 0, screen_h = 0;
static uint32_t boot_time = 0;
static int events_fd = -1;
static int canvas_h = 0,canvas_w = 0;
static uint32_t *fb_mem = NULL;
static int canvas_x0 = 0,canvas_y0 = 0;

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
  
  sscanf(buf, "WIDTH:%d\nHEIGHT:%d", &screen_w, &screen_h);
  close(fd);

  if (*w == 0) *w = screen_w;
  if (*h == 0) *h = screen_h;
  canvas_w = *w; canvas_h = *h;

  canvas_x0 = (screen_w - canvas_w) / 2;
  canvas_y0 = (screen_h - canvas_h) / 2;

  if (fbdev == -1) {
    fbdev = open("/dev/fb", 0, 0);
  }
  fb_mem = (uint32_t *)mmap(NULL, screen_w * screen_h * 4, PROT_WRITE, MAP_SHARED, fbdev, 0);
  printf("NDL: fb_mem address is %p\n", fb_mem);
  printf("NDL: Screen %d x %d, Canvas %d x %d\n", screen_w, screen_h, *w, *h);
}
/*
void NDL_DrawRect(uint32_t *pixels, int x, int y, int w, int h) {
  int fd = fbdev;
  for (int i = 0; i < h; i++) {
    uint32_t offset = ((canvas_y0 + y + i) * screen_w + (canvas_x0 + x)) * 4;
    lseek(fd, offset, SEEK_SET);
    write(fd, pixels + i * w, w * 4);
  }
}
*/
void NDL_DrawRect(uint32_t *pixels, int x, int y, int w, int h) {
  for (int i = 0; i < h; i++) {
    uint32_t *dest = fb_mem + (canvas_y0 + y + i) * screen_w + (canvas_x0 + x);
    uint32_t *src = pixels + i * w;
    printf("copy mem from src %p to dst %p, len = %d\n",src,dest,4*w);
    memcpy(dest, src, w * 4); 
  }
  int area[4] = {canvas_x0 + x, canvas_y0 + y, w, h};
  write(fbdev, area, 0);
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
