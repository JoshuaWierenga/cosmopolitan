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
  // TODO(joshua): Handle realpath in chdir
  if (!realpath("/tmp/", path)) {
    perror("realpath failed");
    return 1;
  }
  printf("realpath turned /tmp/ into %s\n", path);
  if (chdir(path)) {
    perror("chdir failed");
    return 1;
  }
  if (!getcwd(path, PATH_MAX)) {
    perror("getcwd failed");
    return 1;
  }
  printf("new cwd: %s\n", path);

  puts("changing cwd to /proc/../proc/self/./// via fchdir");
  // TODO(joshua): Handle realpath in openat
  if (!realpath("/proc/../proc/self/.///", path)) {
    perror("realpath failed");
    return 1;
  }
  printf("realpath turned /proc/../proc/self/./// into %s\n", path);
  if (!(dir = opendir(path))) {
    perror("opendir failed");
    return 1;
  }
  _defer(closedir, dir);
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
  if (!realpath("././//./", path)) {
    perror("realpath failed");
    return 1;
  }
  printf("realpath turned ././//./ into %s\n", path);
  if (chdir(path)) {
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
