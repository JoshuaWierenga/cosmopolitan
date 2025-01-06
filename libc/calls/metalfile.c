/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ This is free and unencumbered software released into the public domain.      │
│                                                                              │
│ Anyone is free to copy, modify, publish, use, compile, sell, or              │
│ distribute this software, either in source code form or as a compiled        │
│ binary, for any purpose, commercial or non-commercial, and by any            │
│ means.                                                                       │
│                                                                              │
│ In jurisdictions that recognize copyright laws, the author or authors        │
│ of this software dedicate any and all copyright interest in the              │
│ software to the public domain. We make this dedication for the benefit       │
│ of the public at large and to the detriment of our heirs and                 │
│ successors. We intend this dedication to be an overt act of                  │
│ relinquishment in perpetuity of all present and future rights to this        │
│ software under copyright law.                                                │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,              │
│ EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF           │
│ MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.       │
│ IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR            │
│ OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,        │
│ ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR        │
│ OTHER DEALINGS IN THE SOFTWARE.                                              │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/dce.h"
#include "ape/relocations.h"
#include "libc/assert.h"
#include "libc/calls/internal.h"
#include "libc/calls/metalfile.internal.h"
#include "libc/intrin/directmap.h"
#include "libc/intrin/weaken.h"
#include "libc/limits.h"
#include "libc/runtime/pc.internal.h"
#include "libc/runtime/stack.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/at.h"
#include "libc/sysv/consts/dt.h"
#include "libc/sysv/consts/prot.h"
#include "libc/sysv/consts/s.h"
#include "libc/sysv/errfuns.h"

#ifdef __x86_64__

#define MAP_ANONYMOUS_linux 0x00000020
#define MAP_FIXED_linux     0x00000010
#define MAP_SHARED_linux    0x00000001

// Some defines are in metalfile.internal.h
#define ROOT_INO          0
#define DEV_INO           1
#define PROC_INO          2
#define ZIP_INO           4
#define PROC_SELF_INO     5
#define DEV_NULL_INO      6
#define DEV_ZERO_INO      7

__static_yoink("_init_metalfile");

void *__ape_com_base;
size_t __ape_com_size = 0;

struct MetalFile *__metal_files;

ptrdiff_t __metal_cwd_ino = ZIP_INO;

const char ROOT_PATH[]      = "/";
const char DEV_PATH[]       = "/dev";
const char DEV_NULL_PATH[]  = "/dev/null";
const char DEV_ZERO_PATH[]  = "/dev/zero";
const char PROC_PATH[]      = "/proc";
const char PROC_SELF_PATH[] = "/proc/self";
const char TMP_PATH[]       = "/tmp";
const char ZIP_PATH[]       = "/zip";

static void CopyChildPath(ptrdiff_t ino, size_t idx, size_t parentLen, const char *child, size_t childLen) {
  size_t startIdx = parentLen - 1;
  if (child[startIdx] == '/') {
    ++startIdx;
  }
  size_t newPathLen = childLen - startIdx;
  const char *newPath = child + startIdx;

  // For some reason memcpy(__metal_dirs[ino].ents[idx].d_name, newPath, newPathLen) does nothing,
  // it is fine with a literal e.g. memcpy(__metal_dirs[ino].ents[idx].d_name, newPath, 5) though.
  for (size_t i = 0; i < newPathLen; ++i) {
    __metal_files[ino].ents[idx].d_name[i] = newPath[i];
  }
}

#define CopyChildPath(parent, idx, child) \
  CopyChildPath(parent##_INO, idx, sizeof(parent##_PATH), child##_PATH, sizeof(child##_PATH))

#define NUMARGS(type, ...)  (sizeof((type[]){(type){0}, ##__VA_ARGS__})/sizeof(type)-1)
#define AddRootFsDir(ino, mode, path, ...) \
  __metal_files[ino] = (struct MetalFile){ \
    kMetalDir, mode, ino, path, 0, NUMARGS(struct dirent, __VA_ARGS__), \
    NUMARGS(struct dirent, __VA_ARGS__), {__VA_ARGS__} \
  }; \
  CheckLargeStackAllocation(__metal_files + ino, sizeof(*__metal_files))

static void InitializeDirs(void) {
  uint32_t rootFsDirMode = S_IRUSR | S_IXUSR | S_IFDIR;

#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wframe-larger-than="
  // / -> /dev, /proc, /tmp, /zip
  AddRootFsDir(ROOT_INO, rootFsDirMode, ROOT_PATH,
    {DEV_INO,         2, sizeof(*__metal_files[ROOT_INO].ents), DT_DIR},
    {PROC_INO,        3, sizeof(*__metal_files[ROOT_INO].ents), DT_DIR},
    {kMetalTmpDirIno, 4, sizeof(*__metal_files[ROOT_INO].ents), DT_DIR},
    {ZIP_INO,         5, sizeof(*__metal_files[ROOT_INO].ents), DT_DIR},
  );
  CheckLargeStackAllocation(__metal_files[ROOT_INO].ents + 0, sizeof(*__metal_files[ROOT_INO].ents));
  CheckLargeStackAllocation(__metal_files[ROOT_INO].ents + 1, sizeof(*__metal_files[ROOT_INO].ents));
  CheckLargeStackAllocation(__metal_files[ROOT_INO].ents + 2, sizeof(*__metal_files[ROOT_INO].ents));
  CheckLargeStackAllocation(__metal_files[ROOT_INO].ents + 3, sizeof(*__metal_files[ROOT_INO].ents));
  CopyChildPath(ROOT, 0, DEV);
  CopyChildPath(ROOT, 1, PROC);
  CopyChildPath(ROOT, 2, TMP);
  CopyChildPath(ROOT, 3, ZIP);

  // /dev -> /dev/null, /dev/zero
  AddRootFsDir(DEV_INO, rootFsDirMode, DEV_PATH,
    {DEV_NULL_INO, 2, sizeof(*__metal_files[DEV_INO].ents), DT_CHR},
    {DEV_ZERO_INO, 3, sizeof(*__metal_files[DEV_INO].ents), DT_CHR},
  );
  CheckLargeStackAllocation(__metal_files[DEV_INO].ents + 0, sizeof(*__metal_files[DEV_INO].ents));
  CheckLargeStackAllocation(__metal_files[DEV_INO].ents + 1, sizeof(*__metal_files[DEV_INO].ents));
  CopyChildPath(DEV, 0, DEV_NULL);
  CopyChildPath(DEV, 1, DEV_ZERO);

  // /proc -> /proc/self
  AddRootFsDir(PROC_INO, rootFsDirMode, PROC_PATH,
    {PROC_SELF_INO, 2, sizeof(*__metal_files[PROC_INO].ents), DT_DIR}
  );
  CheckLargeStackAllocation(__metal_files[PROC_INO].ents + 0, sizeof(*__metal_files[PROC_INO].ents));
  CopyChildPath(PROC, 0, PROC_SELF);

  // /proc/self -> /proc/self/exec
  AddRootFsDir(PROC_SELF_INO, rootFsDirMode, PROC_SELF_PATH,
    {kMetalProcSelfExeIno, 2, sizeof(*__metal_files[PROC_SELF_INO].ents), DT_CHR}
  );
  CheckLargeStackAllocation(__metal_files[PROC_SELF_INO].ents + 0, sizeof(*__metal_files[PROC_SELF_INO].ents));
  (CopyChildPath)(PROC_SELF_INO, 0, sizeof(PROC_SELF_PATH), APE_COM_NAME, sizeof(APE_COM_NAME));

  // /tmp, managed seperately in metalfile.tmp.c
  AddRootFsDir(kMetalTmpDirIno, S_IWUSR | rootFsDirMode, TMP_PATH);

  // /zip, managed seperately
  AddRootFsDir(ZIP_INO, rootFsDirMode, ZIP_PATH);
#pragma GCC pop_options
}

// TODO: Use for /proc/self/exe
static void InitializeFiles(void) {
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wframe-larger-than="
  __metal_files[DEV_NULL_INO] = (struct MetalFile){
      kMetalFile, S_IRUSR | S_IWUSR | S_IFCHR, DEV_NULL_INO, DEV_NULL_PATH, 0
  };
  CheckLargeStackAllocation(__metal_files + DEV_NULL_INO, sizeof(*__metal_files));

  __metal_files[DEV_ZERO_INO] = (struct MetalFile){
      kMetalFile, S_IRUSR | S_IWUSR | S_IFCHR, DEV_ZERO_INO, DEV_ZERO_PATH, 0
  };
  CheckLargeStackAllocation(__metal_files + DEV_ZERO_INO, sizeof(*__metal_files));
#pragma GCC pop_options
}

textstartup void InitializeMetalFile(void) {
  if (!IsMetal()) {
    return;
  }

  if ( _weaken(__ape_com_sectors)) {
    /*
     * Copy out a pristine image of the program — before the program might
     * decide to modify its own .data section.
     *
     * This code is included if a symbol "file:/proc/self/exe" is defined
     * (see libc/calls/metalfile.internal.h & libc/calls/metalfile_init.S).
     * The zipos code will automatically arrange to do this.  Alternatively,
     * user code can __static_yoink this symbol.
     */
    size_t size = (size_t)__ape_com_sectors * 512;
    __ape_com_size = _MetalAllocate(size, &__ape_com_base);
    memcpy(__ape_com_base, (void *)(BANE + IMAGE_BASE_PHYSICAL), size);
    // TODO(tkchia): LIBC_CALLS doesn't depend on LIBC_VGA so references
    //               to its functions need to be weak
    // KINFOF("%s @ %p,+%#zx", APE_COM_NAME, __ape_com_base, size);
  }

  size_t size = kMetalHardcodedFileCount * sizeof(*__metal_files);
  _MetalAllocate(size, (void **)&__metal_files);

  InitializeDirs();
  InitializeFiles();
}

size_t _MetalAllocate(size_t size, void **addr) {
  size_t full_size;
  full_size = ROUNDUP(size, 4096);
  *addr = sys_mmap_metal(NULL, full_size, PROT_READ | PROT_WRITE,
                         MAP_SHARED_linux | MAP_ANONYMOUS_linux, -1, 0);
  npassert(*addr != (void *)-1);
  return full_size;
}

char *_MetalFullPath(int dirfd, const char *path) {
  struct Fd *fds = 0;
  struct MetalFile *file;
  char abs_path[PATH_MAX];
  static char full_path[PATH_MAX];

  if (dirfd != AT_FDCWD && path[0] != '/') {
    if (dirfd < 0 || dirfd >= g_fds.n) {
      ebadf();
      return 0;
    }

    fds = &g_fds.p[dirfd];
    if (fds->kind != kFdFile) {
      enotdir();
      return 0;
    }

    file = (struct MetalFile *)fds->handle;
    if (file->type != kMetalDir) {
      enotdir();
      return 0;
    }

    if (file->idx >= kMetalHardcodedFileCount || !__metal_files[file->idx].path) {
      ebadf();
      return 0;
    }

    // TODO(joshua): Check if this should be file->idx
    strlcpy(abs_path, __metal_files[__metal_cwd_ino].path, PATH_MAX);
    strlcat(abs_path, "/", PATH_MAX);
    strlcat(abs_path, path, PATH_MAX);
  }

  if (!_MetalPath(fds ? abs_path : path, full_path)) {
    enoent();
    return 0;
  }
  return full_path;
}

#endif /* __x86_64__ */
