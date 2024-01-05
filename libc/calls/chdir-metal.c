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

int sys_chdir_metal(const char *file) {
  char path[PATH_MAX];
  if (!_MetalPath(file, path)) {
    return enoent();
  }
  for (ptrdiff_t i = 0; i < kMetalDirCount; ++i) {
    if (!__metal_dirs[i].path || strcmp(path, __metal_dirs[i].path) != 0) {
      continue;
    }
    __metal_cwd_ino = i;
    return 0;
  }
  if (strcmp(path, APE_COM_NAME) == 0) {
    return enotdir();
  }
  if (__metal_tmpfiles &&
      strncmp(__metal_dirs[kMetalTmpDirIno].path, path, 4) == 0 &&
      path[4] == '/' && path[5] != 0 && strchr(path + 5, '/') == NULL) {
    for (ptrdiff_t i = 0; i < __metal_tmpfiles_max; ++i) {
      if (!__metal_tmpfiles[i].deleted &&
          strcmp(path + 5, __metal_tmpfiles[i].name) == 0) {
        return enotdir();
      }
    }
  }
  return enoent();
}
