#include "libc/calls/calls.h"
#include "libc/calls/struct/iovec.h"
#include "libc/runtime/runtime.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"

void vector_test(void) {
  puts("vector test");

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
}

void offset_vector_test(void) {
  puts("\noffset vector test");

  int fd;
  if ((fd = tmpfd()) == -1) {
    perror("Creating tmp file failed");
    exit(1);
  }

  char *out_buf1 = "Test 2, ";
  char *out_buf2 = "Test 3";
  char *out_buf3 = "Test 1, ";
  struct iovec out_iov[3] = {
    {out_buf1, strlen(out_buf1)},
    {out_buf2, strlen(out_buf2)},
  };
  size_t out_buf_len3 = strlen(out_buf3);
  ssize_t bytes_written = pwritev(fd, out_iov, 2, out_buf_len3);
  bytes_written += write(fd, out_buf3, out_buf_len3);
  if (bytes_written != out_iov[0].iov_len + out_iov[1].iov_len + out_buf_len3) {
    perror("Write failed");
    exit(1);
  }
  printf("Wrote at positions 0 to %d: %.*s\n", out_buf_len3 - 1,
    out_buf_len3, out_buf3);
  printf("Wrote at positions %d to %d: %.*s%.*s\n",
    out_buf_len3, out_buf_len3 + out_iov[0].iov_len + out_iov[1].iov_len - 1,
    out_iov[0].iov_len, out_buf1,
    out_iov[1].iov_len, out_buf2);

  int64_t pos = lseek(fd, 0, SEEK_SET);
  if (pos != 0) {
    perror("Moving to beginning failed");
    exit(1);
  }

  char in_buf1[6];
  char in_buf2[8];
  char in_buf3[6];
  struct iovec in_iov[2] = {
    {in_buf1, sizeof(in_buf1)},
    {in_buf2, sizeof(in_buf2)},
  };
  size_t in_buf_len3 = sizeof(in_buf3);
  size_t bytes_read = preadv(fd, in_iov, 2, in_buf_len3 + 2);
  bytes_read += read(fd, in_buf3, in_buf_len3);
  if (bytes_read == in_iov[0].iov_len + in_iov[1].iov_len + in_buf_len3) {
    printf("Final file contents from 0 to %d(inclusive): %.*s\n",
      in_buf_len3 - 1, in_buf_len3, in_buf3);
    printf("Final file contents from %d to %d: %.*s\n", in_buf_len3 + 2,
      in_iov[0].iov_len + in_buf_len3 + 1, in_iov[0].iov_len, in_buf1);
    printf("Final file contents from %d to %d: %.*s\n",
      in_iov[0].iov_len + in_buf_len3 + 4,
      in_iov[0].iov_len + in_iov[1].iov_len + in_buf_len3 + 1,
      in_iov[1].iov_len - 2, in_buf2 + 2);
  } else {
    perror("Read failed");
    exit(1);
  }

  close(fd);
}

int main(void) {
  vector_test();
  offset_vector_test();
  return 0;
}
