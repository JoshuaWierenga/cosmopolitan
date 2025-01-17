/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2023 Justine Alexandra Roberts Tunney                              │
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
#include "libc/calls/metalfile.internal.h"
#include "libc/calls/struct/stat.h"
#include "libc/calls/struct/stat.internal.h"
#include "libc/intrin/weaken.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/at.h"
#include "libc/sysv/consts/s.h"
#include "libc/sysv/errfuns.h"

#ifdef __x86_64__

// TODO(joshua): Support tmp files
// TODO: Set st_dev, st_uid, st_gid and st_blocks
int sys_fstatat_metal(int dirfd, const char *path, struct stat *st, int flags) {
  char *fullPath;
  struct MetalFile *fileInfo;
  if (!path || !st) {
    return efault();
  };
  if (!__metal_files) {
    return enxio();
  }
  if (!(fullPath = _MetalFullPath(dirfd, path))) {
    return enoent();
  }
  bzero(st, sizeof(*st));
  st->st_blksize = 1;
  if (strcmp(fullPath, APE_COM_NAME) == 0) {
    if (!_weaken(__ape_com_sectors)) {
      return enxio();
    }
    st->st_ino = kMetalProcSelfExeIno;
    st->st_nlink = 1;
    st->st_mode = S_IRUSR | S_IFREG;
    st->st_size = __ape_com_size;
    return 0;
  }
  for (ptrdiff_t i = 0; i < kMetalHardcodedFileCount; ++i) {
    fileInfo = __metal_files + i;
    if (!fileInfo->path || strcmp(fullPath, fileInfo->path) != 0) {
      continue;
    }
    st->st_ino = __metal_files[i].idx;
    st->st_mode = __metal_files[i].mode;
    st->st_size = __metal_files[i].size;
    switch (fileInfo->type) {
      case kMetalDir:
        st->st_nlink = 2;
        return 0;
      case kMetalFile:
        st->st_nlink = 1;
        return 0;
      case kMetalTmp:
        return enosys();
      default:
        return enoent();
    }
  }
  return enoent();
}

#endif /* __x86_64__ */
