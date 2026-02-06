#include <unistd.h>

int main() {
  char *str1 = "Native write 1\n";
  write(1, str1, 15); // 直接调用，不走 printf

  char *str2 = "Trying open...\n";
  write(1, str2, 15);

  int fd = open("/dev/serial", 0, 0); // 看看 ID=2 会不会出现
  
  if (fd > 0) write(1, "Success\n", 8);
  else write(1, "Fail\n", 5);

  return 0;
}