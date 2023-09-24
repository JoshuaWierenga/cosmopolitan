/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
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
#include "libc/calls/struct/stat.h"
#include "libc/dce.h"
#include "libc/errno.h"
#include "libc/fmt/itoa.h"
#include "libc/limits.h"
#include "libc/runtime/runtime.h"
#include "libc/stdio/rand.h"
#include "libc/stdio/stdio.h"
#include "libc/sysv/consts/at.h"
#include "libc/sysv/consts/s.h"
#include "libc/testlib/testlib.h"

char p[2][PATH_MAX];
struct stat st;

void SetUpOnce(void) {
  testlib_enable_tmp_setup_teardown();
  ASSERT_SYS(0, 0, pledge("stdio rpath wpath cpath fattr", 0));
}

TEST(symlink, enoent) {
  // The second symlink call gives EPERM on windows
  // need 2 (or 0x02 or '\2' or ENOENT) =
  //  got 12 (or 0x0c or '\14' or EPERM)
  // https://github.com/jart/cosmopolitan/blob/18bb588/libc/calls/symlinkat-nt.c#L89?
  if (IsWindows()) return;
  ASSERT_SYS(ENOENT, -1, symlink("o/foo", ""));
  ASSERT_SYS(ENOENT, -1, symlink("o/foo", "o/bar"));
}

TEST(symlinkat, enotdir) {
  // The symlink call gives EPERM on windows
  // need 3 (or 0x03 or '\3' or ENOTDIR) =
  //  got 12 (or 0x0c or '\14' or EPERM)
  // https://github.com/jart/cosmopolitan/blob/18bb588/libc/calls/symlinkat-nt.c#L89?
  if (IsWindows()) return;
  ASSERT_SYS(0, 0, close(creat("yo", 0644)));
  ASSERT_SYS(ENOTDIR, -1, symlink("hrcue", "yo/there"));
}

TEST(symlinkat, test) {
  // The symlink call gives EPERM on windows
  // need 0 (or 0x0 or '\0') =
  //  got -1 (or 0xffffffffffffffff)
  // The second issymlink call gives EPERM on windows
  // need 1 (or 0x01 or '\1' or ENOSYS)
  //  got 0 (or 0x0 or '\0')
  // The second lstat call gives ENOENT on windows
  // need 0 (or 0x0 or '\0') =
  //  got -1 (or 0xffffffffffffffff)
  // The second S_ISLNK macro gives ENOENT on windows
  // need 1 (or 0x01 or '\1' or ENOSYS)
  //  got 0 (or 0x0 or '\0')
  // The stat call gives ENOENT on windows
  // need 0 (or 0x0 or '\0') =
  //  got -1 (or 0xffffffffffffffff)
  // https://github.com/jart/cosmopolitan/blob/18bb588/libc/calls/symlinkat-nt.c#L89?
  if (IsWindows()) return;
  sprintf(p[0], "%s.%d", program_invocation_short_name, rand());
  sprintf(p[1], "%s.%d", program_invocation_short_name, rand());

  EXPECT_EQ(0, touch(p[0], 0644));
  EXPECT_EQ(0, symlink(p[0], p[1]));

  // check the normal file
  EXPECT_FALSE(issymlink(p[0]));
  EXPECT_EQ(0, lstat(p[0], &st));
  EXPECT_FALSE(S_ISLNK(st.st_mode));

  // check the symlink file
  EXPECT_TRUE(issymlink(p[1]));
  EXPECT_EQ(0, lstat(p[1], &st));
  EXPECT_TRUE(S_ISLNK(st.st_mode));

  // symlink isn't a symlink if we use it normally
  EXPECT_EQ(0, stat(p[1], &st));
  EXPECT_FALSE(S_ISLNK(st.st_mode));
}
