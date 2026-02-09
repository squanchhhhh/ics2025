#include <curses.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#define SDL_malloc  malloc
#define SDL_free    free
#define SDL_realloc realloc

#define SDL_STBIMAGE_IMPLEMENTATION
#include "SDL_stbimage.h"

SDL_Surface* IMG_Load_RW(SDL_RWops *src, int freesrc) {
  assert(src->type == RW_TYPE_MEM);
  assert(freesrc == 0);
  return NULL;
}
/*
用libc中的文件操作打开文件, 并获取文件大小size
申请一段大小为size的内存区间buf
将整个文件读取到buf中
将buf和size作为参数, 调用STBIMG_LoadFromMemory(), 它会返回一个SDL_Surface结构的指针
关闭文件, 释放申请的内存
返回SDL_Surface结构指针
*/
SDL_Surface* IMG_Load(const char *filename) {
  int fd = open(filename, 0);
  if (fd < 0) {
    printf("Failed to open file: %s\n", filename); 
    return NULL;
  }

  int size = lseek(fd, 0, SEEK_END);
  if (size <= 0) { 
    close(fd); 
    return NULL; 
  }
  
  lseek(fd, 0, SEEK_SET);

  char *buf = malloc(size);
  if (!buf) {
    close(fd);
    return NULL;
  }

  int n = read(fd, buf, size);
  if (n != size) {
    Log("Read error: expected %d, got %d", size, n);
  }

  SDL_Surface *result = STBIMG_LoadFromMemory(buf, size);

  free(buf);
  close(fd);
  return result;
}
int IMG_isPNG(SDL_RWops *src) {
  return 0;
}

SDL_Surface* IMG_LoadJPG_RW(SDL_RWops *src) {
  return IMG_Load_RW(src, 0);
}

char *IMG_GetError() {
  return "Navy does not support IMG_GetError()";
}
