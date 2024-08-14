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
#include "libc/calls/metalfile.internal.h"
#include "libc/calls/internal.h"
#include "libc/intrin/directmap.h"
#include "libc/intrin/kprintf.h"
#include "libc/intrin/weaken.h"
#include "libc/limits.h"
#include "libc/macros.h"
#include "libc/runtime/pc.internal.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/at.h"
#include "libc/sysv/consts/dt.h"
#include "libc/sysv/consts/prot.h"
#include "libc/sysv/errfuns.h"

#ifdef __x86_64__

#define MAP_ANONYMOUS_linux 0x00000020
#define MAP_FIXED_linux     0x00000010
#define MAP_SHARED_linux    0x00000001

// Some defines are in metalfile.internal.h
#define ROOT_INO          0
#define PROC_INO          1
#define ZIP_INO           3
#define PROC_SELF_INO     4
#define PROC_SELF_EXE_INO 5

__static_yoink("_init_metalfile");

void *__ape_com_base;
size_t __ape_com_size = 0;

struct MetalDir *__metal_dirs;

ptrdiff_t __metal_cwd_ino = ZIP_INO;

textstartup void InitializeMetalFile(void) {
  size_t size;
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
    size = (size_t)__ape_com_sectors * 512;
    __ape_com_size = _MetalAllocate(size, &__ape_com_base);
    memcpy(__ape_com_base, (void *)(BANE + IMAGE_BASE_PHYSICAL), size);
    // TODO(tkchia): LIBC_CALLS doesn't depend on LIBC_VGA so references
    //               to its functions need to be weak
    // KINFOF("%s @ %p,+%#zx", APE_COM_NAME, __ape_com_base, size);
  }

  size = kMetalDirCount * sizeof(*__metal_dirs) + sizeof("/") +
    sizeof("/proc") + sizeof("/proc/self") + sizeof("/tmp") + sizeof("/zip");
  _MetalAllocate(size, (void **)&__metal_dirs);

  char *strs = (char *)__metal_dirs + kMetalDirCount * sizeof(*__metal_dirs);
  char *root_path = strs;
  strs[0] =  '/';
  strs[1] =  0;

  char *proc_path = strs + 2;
  strs[2] =  '/';
  char *proc_name = strs + 3;
  strs[3] =  'p';
  strs[4] =  'r';
  strs[5] =  'o';
  strs[6] =  'c';
  strs[7] = 0;

  char *proc_self_path = strs + 8;
  strs[8] =  '/';
  strs[9] =  'p';
  strs[10] = 'r';
  strs[11] = 'o';
  strs[12] = 'c';
  strs[13] = '/';
  char *self_name = strs + 14;
  strs[14] = 's';
  strs[15] = 'e';
  strs[16] = 'l';
  strs[17] = 'f';
  strs[18] = 0;

  char *tmp_path = strs + 19;
  strs[19] = '/';
  char *tmp_name = strs + 20;
  strs[20] = 't';
  strs[21] = 'm';
  strs[22] = 'p';
  strs[23] = 0;

  char *zip_path = strs + 24;
  strs[24] = '/';
  char *zip_name = strs + 25;
  strs[25] = 'z';
  strs[26] = 'i';
  strs[27] = 'p';
  strs[28] = 0;

  // / -> /proc, /tmp, /zip
  __metal_dirs[ROOT_INO] = (struct MetalDir){root_path, 3, 3, {
      {PROC_INO,        2, sizeof(*__metal_dirs), DT_DIR},
      {kMetalTmpDirIno, 3, sizeof(*__metal_dirs), DT_DIR},
      {ZIP_INO,         4, sizeof(*__metal_dirs), DT_DIR},
  }};
  memcpy(__metal_dirs[ROOT_INO].ents[0].d_name, proc_name, sizeof("proc"));
  memcpy(__metal_dirs[ROOT_INO].ents[1].d_name, tmp_name, sizeof("tmp"));
  memcpy(__metal_dirs[ROOT_INO].ents[2].d_name, zip_name, sizeof("zip"));

  // /proc -> /proc/self
  __metal_dirs[PROC_INO] = (struct MetalDir){proc_path, 1, 1, {
      {PROC_SELF_INO, 2, sizeof(*__metal_dirs), DT_DIR}
  }};
  memcpy(__metal_dirs[PROC_INO].ents[0].d_name, self_name, sizeof("self"));

  // /proc/self -> /proc/self/exec
  __metal_dirs[PROC_SELF_INO] = (struct MetalDir){proc_self_path, 1, 1, {
      {PROC_SELF_EXE_INO, 2, sizeof(*__metal_dirs), DT_CHR}
  }};
  __metal_dirs[PROC_SELF_INO].ents[0].d_name[0] = 'e';
  __metal_dirs[PROC_SELF_INO].ents[0].d_name[1] = 'x';
  __metal_dirs[PROC_SELF_INO].ents[0].d_name[2] = 'e';
  __metal_dirs[PROC_SELF_INO].ents[0].d_name[3] = 0;

  // /tmp
  __metal_dirs[kMetalTmpDirIno] = (struct MetalDir){tmp_path, 0, 0};

  // /zip, managed seperately
  __metal_dirs[ZIP_INO] = (struct MetalDir){zip_path, 0, 0};
}

size_t _MetalAllocate(size_t size, void **addr) {
  size_t full_size;
  struct DirectMap dm;
  full_size = ROUNDUP(size, 4096);
  dm = sys_mmap_metal(NULL, full_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED_linux | MAP_ANONYMOUS_linux, -1, 0);
  *addr = dm.addr;
  npassert(*addr != (void *)-1);
  return full_size;
}

// TODO(joshua): Confirm that dirfd is actually a directory
char *_MetalFullPath(int dirfd, const char *file) {
  struct Fd *f = 0;
  struct MetalFile *m;
  char abs_path[PATH_MAX];
  static char full_path[PATH_MAX];

  if (dirfd != AT_FDCWD && file[0] != '/') {
    if (dirfd < 0 || dirfd >= g_fds.n) {
      ebadf();
      return 0;
    }

    f = &g_fds.p[dirfd];
    if (f->kind != kFdFile) {
      enotdir();
      return 0;
    }

    m = (struct MetalFile *)f->handle;
    if (m->type != kMetalDir) {
      enotdir();
      return 0;
    }

    if (m->idx >= kMetalDirCount || !__metal_dirs[m->idx].path) {
      ebadf();
      return 0;
    }

    strlcpy(abs_path, __metal_dirs[__metal_cwd_ino].path, PATH_MAX);
    strlcat(abs_path, "/", PATH_MAX);
    strlcat(abs_path, file, PATH_MAX);
  }

  if (!_MetalPath(f ? abs_path : file, full_path)) {
    enoent();
    return 0;
  }
  return full_path;
}

#endif /* __x86_64__ */
