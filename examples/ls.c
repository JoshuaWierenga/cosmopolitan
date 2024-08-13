#if 0
/*─────────────────────────────────────────────────────────────────╗
│ To the extent possible under law, Justine Tunney has waived      │
│ all copyright and related or neighboring rights to this file,    │
│ as it is written in the following disclaimers:                   │
│   • http://unlicense.org/                                        │
│   • http://creativecommons.org/publicdomain/zero/1.0/            │
╚─────────────────────────────────────────────────────────────────*/
#endif
#include "libc/calls/calls.h"
#include "libc/calls/struct/dirent.h"
#include "libc/calls/struct/stat.h"
#include "libc/log/check.h"
#include "libc/mem/gc.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/dt.h"
#include "libc/sysv/consts/ok.h"
#include "libc/sysv/consts/s.h"
#include "libc/x/xasprintf.h"

struct stat st;

const char *TypeToString(uint8_t type) {
  switch (type) {
    case DT_UNKNOWN:
      return "DT_UNKNOWN";
    case DT_FIFO:
      return "DT_FIFO";
    case DT_CHR:
      return "DT_CHR";
    case DT_DIR:
      return "DT_DIR";
    case DT_BLK:
      return "DT_BLK";
    case DT_REG:
      return "DT_REG";
    case DT_LNK:
      return "DT_LNK";
    case DT_SOCK:
      return "DT_SOCK";
    default:
      return "UNKNOWN";
  }
}

void List(const char *path) {
  DIR *d;
  struct dirent *e;
  const char *vpath;
  if (strcmp(path, ".") == 0) {
    vpath = "";
  } else if (!endswith(path, "/")) {
    vpath = gc(xasprintf("%s/", path));
  } else {
    vpath = path;
  }
  if (stat(path, &st) != -1) {
    if (S_ISDIR(st.st_mode)) {
      CHECK((d = opendir(path)));
      while ((e = readdir(d))) {
        printf("0x%016x 0x%016x %-10s %s%s\n", e->d_ino, e->d_off,
               TypeToString(e->d_type), vpath, e->d_name);
      }
      closedir(d);
    } else {
      printf("%s\n", path);
    }
  } else {
    fprintf(stderr, "not found: %s\n", path);
  }
}

#include "libc/calls/struct/utsname.h"

#define COSMOPOLITAN_VERSION_STR__(x, y, z) #x "." #y "." #z
#define COSMOPOLITAN_VERSION_STR_(x, y, z)  COSMOPOLITAN_VERSION_STR__(x, y, z)
#define COSMOPOLITAN_VERSION_STR                                            \
  COSMOPOLITAN_VERSION_STR_(__COSMOPOLITAN_MAJOR__, __COSMOPOLITAN_MINOR__, \
                            __COSMOPOLITAN_PATCH__)

int main(int argc, char *argv[]) {
  int i;
  if (argc == 1) {
    if (IsMetal()) {
      printf("Cosmopolitan " COSMOPOLITAN_VERSION_STR);
      if (*MODE) {
        printf(" MODE=" MODE);
      }
      puts("\n");

      List("/");
      putchar('\n');
      List("/proc");
      putchar('\n');
      List("/proc/self");
      putchar('\n');
      List("/proc/self/exe");
      int fd1 = tmpfd();
      int fd2 = tmpfd();
      int fd3 = tmpfd();
      puts("\nCreated three files");
      List("/tmp");
      close(fd2);
      puts("\nClosed second file");
      List("/tmp");
      close(fd1);
      puts("\nClosed first file");
      List("/tmp");
      close(fd3);
      putchar('\n');
      List("/zip");
      putchar('\n');

      char file[] = "/zip/test.txt";
      if (access(file, F_OK) == 0) {
        List("/zip/test.txt");
        FILE *file = fopen("/zip/test.txt", "rb");
        char ch;
        while ((ch = fgetc(file)) != EOF) {
          putchar(ch);
        }
        fclose(file);
      }
    } else {
      List(".");
    }
  } else {
    for (i = 1; i < argc; ++i) {
      List(argv[i]);
    }
  }
  return 0;
}
