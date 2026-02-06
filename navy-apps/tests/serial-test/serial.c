#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main() {
  // 1. 测试标准输出 (FD 1)
  char *hello = "--- Nanos-lite VFS Test Start ---\n";
  write(1, hello, strlen(hello));

  // 2. 测试打开多个设备/文件
  // 假设你的 file_table 里还有别的文件，比如事件或日志
  int fd_serial = open("/dev/serial", 0, 0);
  printf("[Test] Open /dev/serial, got FD: %d\n", fd_serial);

  // 3. 测试通过新 FD 进行写入
  if (fd_serial > 0) {
    char *msg = "Write to /dev/serial via FD successful!\n";
    write(fd_serial, msg, strlen(msg));
  } else {
    printf("[Error] Failed to open /dev/serial\n");
    return -1;
  }

  // 4. (进阶) 测试读取功能 (如果你已经实现了 serial_read)
  // 如果没有实现 read，这里可以先注释掉
  /*
  printf("[Test] Please type something in the terminal...\n");
  char read_buf[32];
  int n = read(fd_serial, read_buf, 31);
  if (n > 0) {
    read_buf[n] = '\0';
    printf("[Test] Read back: %s\n", read_buf);
  }
  */

  // 5. 测试同一个文件打开两次 (应该得到不同的 FD 和不同的 s_idx)
  int fd_serial_2 = open("/dev/serial", 0, 0);
  printf("[Test] Open /dev/serial again, got FD: %d\n", fd_serial_2);
  assert(fd_serial != fd_serial_2); // FD 必须不同

  // 6. 结束测试
  write(1, "--- VFS Test Success ---\n", 25);

  return 0;
}