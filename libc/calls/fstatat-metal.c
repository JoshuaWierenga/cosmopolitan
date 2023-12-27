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

int sys_fstatat_metal(int dirfd, const char *path, struct stat *st, int flags) {
  if (dirfd != AT_FDCWD) return enosys();
  if (!path) return efault();
  if (strcmp(path, APE_COM_NAME) == 0) {
    if (!_weaken(__ape_com_size)) return eopnotsupp();
    bzero(st, sizeof(*st));
    st->st_nlink = 1;
    st->st_size = __ape_com_size;
    st->st_mode = S_IFREG | 0600;
    st->st_blksize = 1;
    return 0;
  }
  if (!_weaken(__metal_dirs)) return eopnotsupp();
  for (size_t i = 0; __metal_dirs[i].path; ++i) {
    if (strcmp(path, __metal_dirs[i].path) != 0) continue;
    bzero(st, sizeof(*st));
    st->st_ino = __metal_dirs[i].ino;
    st->st_nlink = 2;
    st->st_mode = S_IFDIR | 0600;
    st->st_blksize = 1;
    return 0;
  }
  return enoent();
}
