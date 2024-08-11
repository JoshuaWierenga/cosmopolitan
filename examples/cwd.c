#include "libc/calls/calls.h"
#include "libc/calls/struct/dirent.h"
#include "libc/limits.h"
#include "libc/mem/gc.h"
#include "libc/stdio/stdio.h"

int main(void) {
  char path[PATH_MAX];
  DIR *dir;
  if (!getcwd(path, PATH_MAX)) {
    perror("getcwd failed");
    return 1;
  }
  printf("initial cwd: %s\n", path);

  puts("changing cwd to /tmp/ via chdir");
  if (chdir("/tmp/")) {
    perror("chdir failed");
    return 1;
  }
  if (!getcwd(path, PATH_MAX)) {
    perror("getcwd failed");
    return 1;
  }
  printf("new cwd: %s\n", path);

  puts("changing cwd to /proc/../proc/self/./// via fchdir");
  if (!(dir = opendir("/proc/../proc/self/.///"))) {
    perror("opendir failed");
    return 1;
  }
  defer(closedir, dir);
  if (fchdir(dirfd(dir))) {
    perror("fchdir failed");
    return 1;
  }
  if (!getcwd(path, PATH_MAX)) {
    perror("getcwd failed");
    return 1;
  }
  printf("new cwd: %s\n", path);

  puts("changing cwd to ./././//./ via chdir");
  if (chdir("./././//./")) {
    perror("chdir failed");
    return 1;
  }
  if (!getcwd(path, PATH_MAX)) {
    perror("getcwd failed");
    return 1;
  }
  printf("new cwd: %s\n", path);
  return 0;
}
