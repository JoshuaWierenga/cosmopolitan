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
#include "libc/calls/calls.h"
#include "libc/calls/metalfile.internal.h"
#include "libc/intrin/weaken.h"
#include "libc/limits.h"
#include "libc/str/str.h"
#include "libc/sysv/errfuns.h"

int sys_chdir_metal(const char *path) {
  char full_path[PATH_MAX];
  if (_weaken(realpath)) {
    if (!_weaken(realpath)(path, full_path)) {
      return enoent();
    }
  } else {
    strlcpy(full_path, path, PATH_MAX);
    if (path[0] != '/') {
      return enosys();
    }
  }
  for (ptrdiff_t i = 0; i < kMetalDirCount; ++i) {
    if (!__metal_dirs[i].path || strcmp(full_path, __metal_dirs[i].path) != 0) {
      continue;
    }
    __metal_cwd_ino = i;
    return 0;
  }
  if (strcmp(full_path, APE_COM_NAME) == 0) {
    return enotdir();
  }
  for (ptrdiff_t i = 0; i < __metal_tmpfiles_max; ++i) {
    if (__metal_tmpfiles[i] && strcmp(full_path, __metal_tmpfiles[i]) == 0) {
      return enotdir();
    }
  }
  return enoent();
}
