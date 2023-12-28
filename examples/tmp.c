#include "libc/calls/calls.h"
#include "libc/str/str.h"

int main(void) {
  int fd;
  if ((fd = tmpfd()) == -1) {
    perror("Creating tmp file failed");
    exit(1);
  }

  char in_buf[32];
  ssize_t bytes_read = read(fd, in_buf, sizeof(in_buf));
  if (bytes_read > 0) {
    printf("Initial file contents:%s\n", in_buf);
  } else {
    puts("File is initally empty");
  }

  char out_buf[32];
  strcpy(out_buf, "This is a test.");
  size_t out_buf_len = strlen(out_buf) + 1;
  ssize_t bytes_written = write(fd, out_buf, strlen(out_buf) + 1);
  if (bytes_written != out_buf_len) {
    perror("Write failed");
    exit(1);
  }
  printf("Wrote: %s\n", out_buf);

  int64_t pos = lseek(fd, 0, SEEK_SET);
  if (pos != 0) {
    perror("Moving to beginning failed");
    exit(1);
  }

  bytes_read = read(fd, in_buf, sizeof(in_buf));
  if (bytes_read > 0) {
    printf("Final file contents:%s\n", in_buf);
  } else {
    puts("File is still empty");
  }

  close(fd);
  return 0;
}
