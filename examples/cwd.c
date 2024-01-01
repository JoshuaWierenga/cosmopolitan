#include "libc/calls/calls.h"
#include "libc/calls/struct/dirent.h"
#include "libc/limits.h"
#include "libc/stdio/stdio.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/at.h"
#include "libc/sysv/consts/o.h"

int main(void) {
  char cwd[PATH_MAX];
  DIR *dir;
  getcwd(cwd, PATH_MAX);
  printf("initial cwd: %s\n", cwd);

  puts("changing cwd to /tmp via chdir");
  chdir("/tmp");
  getcwd(cwd, PATH_MAX);
  printf("new cwd: %s\n", cwd);

  puts("changing cwd to /proc/self via fchdir");
  dir = opendir("/proc/self");
  fchdir(dirfd(dir));
  getcwd(cwd, PATH_MAX);
  printf("new cwd: %s\n", cwd);
  closedir(dir);
  return 0;
}
