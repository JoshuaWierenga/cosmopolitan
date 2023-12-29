#include "libc/calls/calls.h"
#include "libc/calls/struct/iovec.h"
#include "libc/runtime/runtime.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"

int main(void) {
  int fd;
  if ((fd = tmpfd()) == -1) {
    perror("Creating tmp file failed");
    exit(1);
  }

  char in_buf1[32];
  ssize_t bytes_read = read(fd, in_buf1, sizeof(in_buf1));
  if (bytes_read > 0) {
    printf("Initial file contents:%s\n", in_buf1);
  } else {
    puts("File is initally empty");
  }

  char *out_buf1 = "Test 1, ";
  char *out_buf2 = "Test 2, ";
  char *out_buf3 = "Test 3";
  struct iovec out_iov[3] = {
    {out_buf1, strlen(out_buf1)},
    {out_buf2, strlen(out_buf2)},
    {out_buf3, strlen(out_buf3)},
  };

  ssize_t bytes_written = writev(fd, out_iov, 3);
  if (bytes_written != out_iov[0].iov_len + out_iov[1].iov_len + out_iov[2].iov_len) {
    perror("Write failed");
    exit(1);
  }
  printf("Wrote: %.*s%.*s%.*s\n",
      out_iov[0].iov_len, out_buf1,
      out_iov[1].iov_len, out_buf2,
      out_iov[2].iov_len, out_buf3);

  int64_t pos = lseek(fd, 0, SEEK_SET);
  if (pos != 0) {
    perror("Moving to beginning failed");
    exit(1);
  }

  char in_buf2[32];
  char in_buf3[32];
  struct iovec in_iov[3] = {
    {in_buf1, strlen(out_buf1)},
    {in_buf2, strlen(out_buf2)},
    {in_buf3, strlen(out_buf3)},
  };

  bytes_read = readv(fd, in_iov, 3);
  if (bytes_read == in_iov[0].iov_len + in_iov[1].iov_len + in_iov[2].iov_len) {
    printf("Final file contents: %.*s%.*s%.*s\n",
      in_iov[0].iov_len, in_buf1,
      in_iov[1].iov_len, in_buf2,
      in_iov[2].iov_len, in_buf3);
  } else {
    perror("Read failed");
    exit(1);
  }

  close(fd);
  return 0;
}
