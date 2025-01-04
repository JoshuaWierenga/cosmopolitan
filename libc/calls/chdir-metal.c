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

#ifdef __x86_64__

// TODO(joshua): Check exec permission
int sys_chdir_metal(const char *path) {
  char fullPath[PATH_MAX];
  struct MetalFile *fileInfo;
  if (!__metal_files) {
    return enxio();
  }
  if (!_MetalPath(path, fullPath)) {
    return enoent();
  }
  if (strcmp(fullPath, APE_COM_NAME) == 0) {
    return enotdir();
  }
  for (ptrdiff_t i = 0; i < kMetalHardcodedFileCount; ++i) {
    fileInfo = __metal_files + i;
    if (!fileInfo->path || strcmp(fullPath, fileInfo->path) != 0) {
      continue;
    }
    if (fileInfo->type != kMetalDir) {
      return enotdir();
    }
    __metal_cwd_ino = i;
    return 0;
  }
  if (__metal_tmpfiles &&
      strncmp(__metal_files[kMetalTmpDirIno].path, fullPath, 4) == 0 &&
      fullPath[4] == '/' && fullPath[5] != 0 && strchr(fullPath + 5, '/') == NULL) {
    for (ptrdiff_t i = 0; i < __metal_tmpfiles_max; ++i) {
      if (!__metal_tmpfiles[i].deleted &&
          strcmp(fullPath + 5, __metal_tmpfiles[i].name) == 0) {
        return enotdir();
      }
    }
  }
  return enoent();
}

#endif /* __x86_64__ */
