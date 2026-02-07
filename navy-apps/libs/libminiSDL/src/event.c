#include <NDL.h>
#include <SDL.h>

#define keyname(k) #k,

static const char *keyname[] = {
  "NONE",
  _KEYS(keyname)
};

int SDL_PushEvent(SDL_Event *ev) {
  return 0;
}

int SDL_PollEvent(SDL_Event *ev) {
  char buf[64];
  if (NDL_PollEvent(buf, sizeof(buf))) {
    char type[8];
    char key[16];
    if (sscanf(buf, "%s %s", type, key) == 2) {
      if (strcmp(type, "kd") == 0){
        ev->key.type = SDL_KEYDOWN;
      }else{
        ev->key.type = SDL_KEYUP;
      }
      ev->type = ev->key.type;
      for (int i = 0; i < sizeof(keyname) / sizeof(keyname[0]); i++) {
        if (strcmp(key, keyname[i]) == 0) {
          ev->key.keysym.sym = i;
          return 1;
        }
      }
    }
  }
  return 0;
}

int SDL_WaitEvent(SDL_Event *event) {
 while (SDL_PollEvent(event) == 0); 
  return 1;
}

int SDL_PeepEvents(SDL_Event *ev, int numevents, int action, uint32_t mask) {
  return 0;
}

uint8_t* SDL_GetKeyState(int *numkeys) {
  return NULL;
}
