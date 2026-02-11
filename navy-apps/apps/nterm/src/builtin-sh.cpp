#include <nterm.h>
#include <stdarg.h>
#include <unistd.h>
#include <SDL.h>

char handle_key(SDL_Event *ev);

static void sh_printf(const char *format, ...) {
  static char buf[256] = {};
  va_list ap;
  va_start(ap, format);
  int len = vsnprintf(buf, 256, format, ap);
  va_end(ap);
  term->write(buf, len);
}

static void sh_banner() {
  sh_printf("Built-in Shell in NTerm (NJU Terminal)\n\n");
}

static void sh_prompt() {
  sh_printf("sh> ");
}

static void sh_handle_cmd(const char *cmd) {
  static char buf[128];
  strncpy(buf, cmd, sizeof(buf) - 1);
  
  // 1. 去掉换行符
  char *ptr = strchr(buf, '\n');
  if (ptr) *ptr = '\0';
  if (strlen(buf) == 0) return;

  // 2. 切分字符串 (使用 strtok)
  char *argv[16]; // 最多支持 16 个参数
  int argc = 0;
  
  char *token = strtok(buf, " ");
  while (token != NULL && argc < 15) {
    argv[argc++] = token;
    token = strtok(NULL, " ");
  }
  argv[argc] = NULL; // 按照要求，argv 必须以 NULL 结尾

  // 3. 执行
  // 第一个参数 argv[0] 应该是命令路径本身
  execve(argv[0], argv, NULL);

  // 4. 如果 execve 返回了，说明执行失败
  printf("Command not found: %s\n", argv[0]);
}
void builtin_sh_run() {
  sh_banner();
  sh_prompt();

  while (1) {
    SDL_Event ev;
    if (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_KEYUP || ev.type == SDL_KEYDOWN) {
        const char *res = term->keypress(handle_key(&ev));
        if (res) {
          sh_handle_cmd(res);
          sh_prompt();
        }
      }
    }
    refresh_terminal();
  }
}
