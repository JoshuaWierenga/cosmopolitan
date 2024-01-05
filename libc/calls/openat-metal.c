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
#include "libc/macros.internal.h"
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

static ptrdiff_t find_metal_dir(const char *path) {
  ptrdiff_t i = 0;
  while (i < kMetalDirCount &&
         (!__metal_dirs[i].path || strcmp(path, __metal_dirs[i].path) != 0)) {
    ++i;
  }
  return i < kMetalDirCount ? i : -1;
}

static bool32 is_in_tmp_folder(const char *path) {
  return strncmp(__metal_dirs[kMetalTmpDirIno].path, path, 4) == 0 &&
         path[4] == '/' && path[5] != 0 && strchr(path + 5, '/') == 0;
}

/*static ptrdiff_t find_metal_tmp_file(const char *path) {
  ptrdiff_t i = 0;
  if (!__metal_tmpfiles || !is_in_tmp_folder(path)) {
    return -1;
  }
  while (i < __metal_tmpfiles_max &&
         (!__metal_tmpfiles[i] || strcmp(path + 5, __metal_tmpfiles[i]) != 0)) {
    ++i;
  }
  return i < __metal_tmpfiles_max ? i : -1;
}*/

// TODO(joshua): Support O_APPEND, need to add to sys_write_metal
// TODO(joshua): Support O_CLOEXEC properly? Its useless on metal but posix requires it
// TODO(joshua): Support O_CREAT|O_EXCL? Its useless on metal but posix requires it
// TODO(joshua): Support O_NOATIME? Not required but will be easy once times are recorded
// TODO(joshua): Support O_TRUNC, requires optional O_UNLINK and then only conditionally setting size to zero, currently its implicit for tmp files
// TODO(joshua): Support O_TMPFILE? Not required and probably useless on metal but not hard to do
// TODO(joshua): Make O_UNLINK optional for tmp files
// TODO(joshua): Support O_RDONLY or O_WRONLY tmp files?
// State of flags support:
// O_APPEND, O_COMPRESSED, O_DIRECT, O_DSYNC, O_EXLOCK: Not supported
// O_INDEXED, O_NOATIME, O_RANDOM, O_RSYNC, O_SEARCH: Not supported
// O_SEQUENTIAL, O_SHLOCK, O_SYNC, O_TRUNC, O_TMPFILE, O_VERIFY: Not supported
// O_EXLOCK, O_SEARCH, O_SHLOCK, O_VERIFY == -1?
// O_PATH == O_EXEC?
// O_TMPFILE|O_DIRECTORY != 0?
// O_CLOEXEC: Ignoring which breaks posix compat
// O_NOFOLLOW, O_NONBLOCK, O_TTY_INIT: Ignored due to unsupported features, allowed by posix
// O_CREAT|O_EXCL: O_EXCL is ignored when used correctly
int sys_openat_metal(int dirfd, const char *file, int flags, unsigned mode) {
  int fd;
  struct MetalFile *state;
  char *path;
  ptrdiff_t idx;
  // Not supported but required by posix
  if (flags & (O_APPEND|O_TRUNC)) {
  return enosys();
  }
  // Not supported and either OS or cosmo specific, or optional in posix
  if (flags & (O_COMPRESSED|O_DIRECT|O_DSYNC|O_INDEXED|O_NOATIME|O_RANDOM|
               O_RSYNC|O_SEQUENTIAL|O_SYNC)) {
    return einval();
  }
  // Avoid undefined behaviour
  if (/*((flags & O_ACCMODE) == O_RDONLY && flags & O_TRUNC) ||*/
      (~flags & O_CREAT && flags & O_EXCL)) {
    return einval();
  }
  if (!(path = _MetalFullPath(dirfd, file))) {
    return -1;
  }
  if (strcmp(path, APE_COM_NAME) == 0) {
    // APE_COM_NAME is 0400 or perhaps 0444 but without other users it does not matter
    if ((flags & O_ACCMODE) != O_RDONLY /*|| flags & O_TRUNC*/) {
      return eacces();
    }
    if (flags & (O_CREAT|O_EXCL)) {
      return eexist();
    }
    if (flags & O_DIRECTORY) {
      return enotdir();
    }
    if (!_weaken(__ape_com_sectors)) {
      return enxio();
    }
    if ((state = allocate_metal_file()) <= (struct MetalFile *)0) {
      return enomem(); // enfile?
    }
    state->type = kMetalApe;
    state->base = (char *)__ape_com_base;
    state->size = __ape_com_size;
  // metalfile dirs are 0500
  } else if ((idx = find_metal_dir(path)) != -1) {
    /*if (flags & O_TRUNC) {
      return eacces();
    }*/
    if (flags & (O_CREAT|O_EXCL)) {
      return eexist();
    }
    if ((flags & O_ACCMODE) != O_RDONLY ||
        (~flags & O_DIRECTORY && flags & O_CREAT)) {
      return eisdir();
    }
    if (flags & O_EXEC) {
      return einval();
    }
    if ((state = allocate_metal_file()) <= (struct MetalFile *)0) {
      return enomem(); // enfile?
    }
    state->type = kMetalDir;
    state->idx = idx;
  // tmp dir is 0700
  } else if (is_in_tmp_folder(path)) {
    if ((flags & O_ACCMODE) != O_RDWR || flags & O_DIRECTORY || ~flags & O_UNLINK) {
      return enosys();
    }
    if (~flags & O_CREAT) {
      return enoent();
    }
    if ((state = allocate_metal_file()) <= (struct MetalFile *)0) {
      return enomem(); // enfile?
    }
    if (!_OpenMetalTmpFile(path + 5, state)) {
      return eacces();
    }
  } else if ((flags & O_ACCMODE) != O_RDONLY || flags & (O_CREAT/*|O_TRUNC*/)) {
    return erofs();
  } else {
    return enoent();
  }

  if ((fd = __reservefd(-1)) == -1) {
    // TODO(joshua): unmap state
    emfile();
  }
  g_fds.p[fd].kind = kFdFile;
  g_fds.p[fd].flags = flags;
  g_fds.p[fd].mode = mode;
  g_fds.p[fd].handle = (intptr_t)state;
  return fd;
}

#endif /* __x86_64__ */
