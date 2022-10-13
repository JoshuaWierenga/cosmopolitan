/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2020 Justine Alexandra Roberts Tunney                              │
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
#include "libc/calls/internal.h"
#include "libc/calls/syscall_support-nt.internal.h"
#include "libc/dce.h"
#include "libc/nt/files.h"
#include "libc/nt/nt/file.h"
#include "libc/nt/struct/filenameinformation.h"
#include "libc/str/str.h"
#include "libc/sysv/errfuns.h"

textwindows int sys_fchdir_nt(int dirfd) {
  struct NtIoStatusBlock ntstatusblock;
  struct NtFileNameInformation fni;
  uint32_t len;
  char16_t dir[PATH_MAX];
  if (!__isfdkind(dirfd, kFdFile)) return ebadf();
  // This is not the same as GetFinalPathNameByHandle as it doesn't give a
  // normalised name and FileNormalizedNameInformation was added in windows 8.
  NtQueryInformationFile(g_fds.p[dirfd].handle, &ntstatusblock, &fni,
      sizeof(fni), kNtFileNameInformation);
  len = fni.FileNameLength / sizeof(*fni.FileName);
  // NtFileNameInformation says that FileName is 1 char long but documentation
  // says that the rest of the string follows it in memory
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
  memmove(dir, fni.FileName, ARRAYLEN(dir));
#pragma GCC diagnostic pop
  if (len + 1 + 1 > ARRAYLEN(dir)) return enametoolong();
  if (dir[len - 1] != u'\\') {
    dir[len + 0] = u'\\';
    dir[len + 1] = u'\0';
  }
  if (SetCurrentDirectory(dir)) {
    return 0;
  } else {
    return __winerr();
  }
}
