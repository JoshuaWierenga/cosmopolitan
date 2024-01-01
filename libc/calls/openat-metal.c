/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2021 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/calls/internal.h"
#include "libc/calls/metalfile.internal.h"
#include "libc/intrin/directmap.internal.h"
#include "libc/intrin/weaken.h"
#include "libc/mem/mem.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/map.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/prot.h"
#include "libc/sysv/errfuns.h"

#ifdef __x86_64__

static struct MetalFile *allocate_metal_file(void) {
  if (!_weaken(calloc) || !_weaken(free)) {
    struct DirectMap dm;
    dm = sys_mmap_metal(NULL, ROUNDUP(sizeof(struct MetalFile), 4096),
                        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1,
                        0);
    return dm.addr;
  } else {
    return _weaken(calloc)(1, sizeof(struct MetalFile));
  }
}

int sys_openat_metal(int dirfd, const char *file, int flags, unsigned mode) {
  int fd;
  struct MetalFile *state = 0;
  char *path;
  if (!(path = GetFullMetalPath(dirfd, file))) {
    return -1;
  }
  if (strcmp(path, APE_COM_NAME) == 0) {
    if (flags != O_RDONLY) return eacces();
    if (!_weaken(__ape_com_base) || !_weaken(__ape_com_size)) {
      return eopnotsupp();
    }
    if ((state = allocate_metal_file()) <= (struct MetalFile *)0) {
      return enosys();
    }
    state->type = kMetalApe;
    state->base = (char *)__ape_com_base;
    state->size = __ape_com_size;
  } else if (((flags & O_ACCMODE) == O_RDONLY) &&
             (~flags | (O_RDONLY|O_CLOEXEC|O_DIRECTORY|O_NOCTTY))) {
    if (!_weaken(__metal_dirs)) return eopnotsupp();
    for (ptrdiff_t i = 0; i < kMetalDirCount; ++i) {
      if (!__metal_dirs[i].path || strcmp(path, __metal_dirs[i].path) != 0) {
        continue;
      }
      if ((state = allocate_metal_file()) <= (struct MetalFile *)0) {
        return enosys();
      }
      state->type = kMetalDir;
      state->idx = i;
    }
  } else if (flags & (O_RDWR|O_CREAT|O_EXCL|O_UNLINK) &&
             strncmp(__metal_dirs[kMetalTmpDirIno].path, path, 4) == 0 &&
             path[4] == '/' && path[5] != 0 && strchr(path + 5, '/') == NULL) {
    if (!_weaken(__metal_tmpfiles) || !_weaken(__metal_tmpfiles_size)) {
      return eopnotsupp();
    }
    if ((state = allocate_metal_file()) <= (struct MetalFile *)0) {
      return enosys();
    }
    if (!OpenMetalTmpFile(path + 5, state)) return eacces();
  } else {
    return eacces();
  }
  if (!state) return enoent();
  if ((fd = __reservefd(-1)) == -1) enosys();
  g_fds.p[fd].kind = kFdFile;
  g_fds.p[fd].flags = flags;
  g_fds.p[fd].mode = mode;
  g_fds.p[fd].handle = (intptr_t)state;
  return fd;
}

#endif /* __x86_64__ */
