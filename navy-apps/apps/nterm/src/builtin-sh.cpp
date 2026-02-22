#include <SDL.h>
#include <nterm.h>
#include <stdarg.h>
#include <unistd.h>

extern "C" {
  int _wait(int *status);
  int _fork();
  int _execve(const char *fname, char * const argv[], char *const envp[]);
}

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

static void sh_prompt() { sh_printf("sh> "); }

static void sh_handle_cmd(const char *cmd) {
  static char buf[128];
  strncpy(buf, cmd, sizeof(buf) - 1);
  char *ptr = strchr(buf, '\n');
  if (ptr)
    *ptr = '\0';
  if (strlen(buf) == 0)
    return;
  char *argv[16];
  int argc = 0;
  char *token = strtok(buf, " ");
  while (token != NULL && argc < 15) {
    argv[argc++] = token;
    token = strtok(NULL, " ");
  }
  argv[argc] = NULL;
  int pid = _fork(); 
  if (pid < 0) {
    sh_printf("Fork failed\n");
    return;
  }

  if (pid == 0) {
    execve(argv[0], argv, NULL);
    sh_printf("sh: command not found: %s\n", argv[0]);
    _exit(-1); 
  } else {
    int status;
    _wait(&status); 
  }
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
