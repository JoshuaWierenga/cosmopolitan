/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
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
#include "libc/intrin/strace.internal.h"
#include "libc/calls/syscall_support-nt.internal.h"
#include "libc/macros.internal.h"
#include "libc/nt/files.h"
#include "libc/nt/nt/file.h"
#include "libc/nt/struct/filenameinformation.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/at.h"
#include "libc/sysv/errfuns.h"

int __mkntpathat(int dirfd, const char *path, int flags,
                 char16_t file[hasatleast PATH_MAX]) {
  struct NtIoStatusBlock ntstatusblock;
  struct NtFileNameInformation fni;
  char16_t dir[PATH_MAX];
  uint32_t dirlen, filelen;
  if ((filelen = __mkntpath2(path, file, flags)) == -1) return -1;
  if (!filelen) return enoent();
  if (file[0] != u'\\' && dirfd != AT_FDCWD) { /* ProTip: \\?\C:\foo */
    if (!__isfdkind(dirfd, kFdFile)) return ebadf();
    // This is not the same as GetFinalPathNameByHandle as it doesn't give a
    // normalised name and FileNormalizedNameInformation was added in windows 8.
    NtQueryInformationFile(g_fds.p[dirfd].handle, &ntstatusblock, &fni,
        sizeof(fni), kNtFileNameInformation);
    dirlen = fni.FileNameLength / sizeof(*fni.FileName);
    // NtFileNameInformation says that FileName is 1 char long but documentation
    // says that the rest of the string follows it in memory
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
    memmove(dir, fni.FileName, ARRAYLEN(dir));
#pragma GCC diagnostic pop
    if (!dirlen) return __winerr();
    if (dirlen + 1 + filelen + 1 > ARRAYLEN(dir)) {
      STRACE("path too long: %#.*hs\\%#.*hs", dirlen, dir, filelen, file);
      return enametoolong();
    }
    dir[dirlen] = u'\\';
    memcpy(dir + dirlen + 1, file, (filelen + 1) * sizeof(char16_t));
    memcpy(file, dir, (dirlen + 1 + filelen + 1) * sizeof(char16_t));
    return dirlen + 1 + filelen;
  } else {
    return filelen;
  }
}
