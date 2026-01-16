#include <am.h>
#include <nemu.h>

#define KEYDOWN_MASK 0x8000

void __am_input_keybrd(AM_INPUT_KEYBRD_T *kbd) {
  uint32_t key = inl(KBD_ADDR);

  if (key == 0) {
    char test = 'a';
    putch(test);
    kbd->keydown = 0;
    kbd->keycode = AM_KEY_NONE;
  } else {
    kbd->keydown = (key & KEYDOWN_MASK) ? 1 : 0;
    kbd->keycode = key & ~KEYDOWN_MASK;
  }
}
void __am_input_config(AM_INPUT_CONFIG_T *cfg) {
  cfg->present = true;
}