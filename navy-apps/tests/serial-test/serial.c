#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main() {
  const char *msg1 = "Direct write to fd 1 (stdout) successful!\n";
  write(1, msg1, strlen(msg1));

  int fd = open("/dev/serial", 0, 0);
  if (fd > 0) {
    const char *msg2 = "Write to /dev/serial via open() successful!\n";
    write(fd, msg2, strlen(msg2));
    close(fd);
  } else {
    printf("Optional: open /dev/serial failed (this is okay if open is not fully implemented)\n");
  }
  for (int i = 0; i < 5; i++) {
    printf("Serial test loop: %d/5\n", i + 1);
  }

  return 0;
}