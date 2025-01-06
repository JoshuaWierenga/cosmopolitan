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
#include "libc/intrin/directmap.h"
#include "libc/intrin/weaken.h"
#include "libc/macros.h"
#include "libc/mem/mem.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/map.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/prot.h"
#include "libc/sysv/consts/s.h"
#include "libc/sysv/errfuns.h"

#ifdef __x86_64__

// TODO: Merge with _MetalAllocate in metalfile.c?
static struct MetalOpenFile *allocate_metal_file(void) {
  if (!_weaken(calloc) || !_weaken(free)) {
    return sys_mmap_metal(NULL, ROUNDUP(sizeof(struct MetalOpenFile), 4096),
                          PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1,
                          0);
  } else {
    return _weaken(calloc)(1, sizeof(struct MetalOpenFile));
  }
}

static ptrdiff_t is_metal_dir(const char *path) {
  ptrdiff_t i = 0;
  struct MetalFile *fileInfo;
  for (; i < kMetalHardcodedFileCount; ++i) {
    fileInfo = __metal_files + i;
    if (fileInfo->type == kMetalDir && fileInfo->path &&
        strcmp(path, fileInfo->path) == 0) {
      break;
    }
  }
  return i < kMetalHardcodedFileCount ? i : -1;
}

static bool32 is_in_tmp_folder(const char *path) {
  return strncmp(__metal_files[kMetalTmpDirIno].path, path, 4) == 0 &&
         path[4] == '/' && path[5] != 0 && strchr(path + 5, '/') == 0;
}

static ptrdiff_t is_tmp_file(const char *path) {
  ptrdiff_t i = 0;
  if (!__metal_tmpfiles || !is_in_tmp_folder(path)) {
    return -1;
  }

  while (i < __metal_tmpfiles_max && (__metal_tmpfiles[i].deleted ||
         strcmp(path + 5, __metal_tmpfiles[i].name) != 0)) {
    ++i;
  }

  return i < __metal_tmpfiles_max ? i : -1;
}

// TODO(joshua): Use in fstat(at)
static struct MetalFile *find_file(const char *path) {
  static struct MetalFile fileInfo;
  if (strcmp(path, APE_COM_NAME) == 0) {
    fileInfo.type = kMetalApe;
    fileInfo.mode = S_IRUSR | S_IFCHR;
  } else if ((fileInfo.idx = is_metal_dir(path)) != -1) {
    fileInfo.type = kMetalDir;
    fileInfo.mode = S_IRUSR | S_IXUSR | S_IFDIR;
    if (fileInfo.idx == kMetalTmpDirIno) {
      fileInfo.mode |= S_IWUSR;
    }
  } else if ((fileInfo.idx = is_tmp_file(path)) != -1) {
    fileInfo.type = kMetalTmp;
    fileInfo.mode = S_IRWXU | S_IFREG;
  } else {
    fileInfo.type = kMetalBad;
  }
  return &fileInfo;
}

// TODO(joshua): Support hardcoded files
// TODO(joshua): Support O_APPEND, need to add to sys_write_metal
// TODO(joshua): Support O_CLOEXEC properly? Its useless on metal but posix requires it
// TODO(joshua): Support O_CREAT|O_EXCL? Its useless on metal but posix requires it
// TODO(joshua): Support O_NOATIME? Not required but will be easy once times are recorded
// TODO(joshua): Support O_TRUNC, requires optional O_UNLINK and then only conditionally setting size to zero, currently its implicit for tmp files
// TODO(joshua): Support O_TMPFILE? Not required and probably useless on metal but not hard to do
// TODO(joshua): Make O_UNLINK optional for tmp files
// TODO(joshua): Support O_RDONLY or O_WRONLY tmp files?
// State of flags support:
// O_APPEND, O_COMPRESSED, O_DIRECT, O_DSYNC, O_EXEC, O_EXLOCK: Not supported
// O_INDEXED, O_NOATIME, O_RANDOM, O_RSYNC, O_SEARCH: Not supported
// O_SEQUENTIAL, O_SHLOCK, O_SYNC, O_TRUNC, O_TMPFILE, O_VERIFY: Not supported
// O_EXLOCK, O_SEARCH, O_SHLOCK, O_VERIFY == -1?
// O_PATH == O_EXEC?
// O_TMPFILE|O_DIRECTORY != 0?
// O_CLOEXEC: Ignoring which breaks posix compat
// O_NOFOLLOW, O_NONBLOCK, O_TTY_INIT: Ignored due to unsupported features, allowed by posix
// O_CREAT|O_EXCL: O_EXCL is currently ignored when used correctly which breaks posix compat
int sys_openat_metal(int dirfd, const char *path, int flags, unsigned mode) {
  char *fullPath;
  bool32 inTmp;
  struct MetalFile *fileInfo;
  struct MetalOpenFile *file;
  int fd;
  if (!__metal_files) {
    return enxio();
  }
  // Not supported but required by posix
  if (flags & (O_APPEND|O_EXEC|O_TRUNC)) {
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
  if (!(fullPath = _MetalFullPath(dirfd, path))) {
    return enoent();
  }
  // TODO(joshua): Find parent dir info as well?
  fileInfo = find_file(fullPath);
  inTmp = is_in_tmp_folder(fullPath);
  if (fileInfo->type != kMetalBad) {
    if (fileInfo->type == kMetalTmp && ((flags & O_ACCMODE) != O_RDWR ||
                                        !(flags && O_UNLINK))) {
      return enosys();
    }
    if (((flags & O_ACCMODE) == O_RDONLY && !(fileInfo->mode & S_IRUSR)) ||
        ((flags & O_ACCMODE) == O_WRONLY && !(fileInfo->mode & S_IWUSR)) ||
        ((flags & O_ACCMODE) == O_RDWR   && !(fileInfo->mode & (S_IRUSR|S_IWUSR))) /*||
        ((flags & O_ACCMODE) == O_EXEC   && !(info->mode & S_IXUSR)) ||
        ((flags & O_TRUNC)               && !(info->mode & S_IWUSR)*/) {
      return eacces();
    }
    if (flags & (O_CREAT|O_EXCL)) {
      return eexist();
    }
    if (fileInfo->type == kMetalDir && (((flags & O_ACCMODE) & O_WRONLY) ||
                                        (flags & O_CREAT &&
                                         !(flags && O_DIRECTORY))/* ||
                                          flags & O_EXEC*/)) {
      return eisdir();
    }
    if (fileInfo->type != kMetalDir && flags & O_DIRECTORY) {
      return enotdir();
    }
    if (fileInfo->type == kMetalApe && !_weaken(__ape_com_sectors)) {
      return enxio();
    }
  } else {
    if (!inTmp /*|| flags & O_TRUNC*/) {
      return eacces();
    }
    if (!(flags && O_CREAT)) {
      return enoent();
    }
  }
  if ((fileInfo->type != kMetalTmp && ((flags & O_ACCMODE) & O_WRONLY /*||
                                       (flags && O_TRUNC)*/)) ||
      (fileInfo->type == kMetalBad && !inTmp && flags & O_CREAT)) {
    return erofs();
  }
  if (fileInfo->type == kMetalApe) {
    if ((file = allocate_metal_file()) <= (struct MetalOpenFile *)0) {
      return enomem(); // enfile?
    }
    file->type = kMetalApe;
    file->base = (uint8_t *)__ape_com_base;
    file->size = __ape_com_size;
  } else if (fileInfo->type == kMetalDir) {
    if ((file = allocate_metal_file()) <= (struct MetalOpenFile *)0) {
      return enomem(); // enfile?
    }
    file->type = kMetalDir;
    file->idx = fileInfo->idx;
  } else if (inTmp) {
    if ((file = allocate_metal_file()) <= (struct MetalOpenFile *)0) {
      return enomem(); // enfile?
    }
    // TODO(joshua): Move some of this back into this file
    if (!_OpenMetalTmpFile(fullPath + 5, file)) {
      return eacces();
    }
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
  g_fds.p[fd].handle = (intptr_t)file;
  return fd;
}

#endif /* __x86_64__ */
