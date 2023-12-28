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
#include "ape/relocations.h"
#include "ape/sections.internal.h"
#include "libc/assert.h"
#include "libc/calls/internal.h"
#include "libc/calls/metalfile.internal.h"
#include "libc/intrin/directmap.internal.h"
#include "libc/intrin/kprintf.h"
#include "libc/intrin/weaken.h"
#include "libc/macros.internal.h"
#include "libc/mem/mem.h"
#include "libc/runtime/pc.internal.h"
#include "libc/runtime/runtime.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/at.h"
#include "libc/sysv/consts/dt.h"
#include "libc/sysv/consts/o.h"
#include "libc/sysv/consts/prot.h"
#include "libc/sysv/errfuns.h"

#ifdef __x86_64__

#define MAP_ANONYMOUS_linux 0x00000020
#define MAP_FIXED_linux     0x00000010
#define MAP_SHARED_linux    0x00000001

#define DIR_COUNT 5
#define EOD {0, -1}
#define ROOT_INO          0
#define PROC_INO          1
#define TMP_INO           2
#define ZIP_INO           3
#define PROC_SELF_INO     4
#define PROC_SELF_EXE_INO 5

__static_yoink("_init_metalfile");

void *__ape_com_base;
size_t __ape_com_size = 0;

struct MetalDirInfo *__metal_dirs;

char **__metal_tmpfiles = NULL;
size_t __metal_tmpfiles_size = 0;

textstartup void InitializeMetalFile(void) {
  size_t size;
  struct DirectMap dm;
  if (IsMetal()) {
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
      size = ROUNDUP((size_t)__ape_com_sectors * 512, 4096);
      void *copied_base;
      dm = sys_mmap_metal(NULL, size, PROT_READ | PROT_WRITE,
                          MAP_SHARED_linux | MAP_ANONYMOUS_linux, -1, 0);
      copied_base = dm.addr;
      npassert(copied_base != (void *)-1);
      memcpy(copied_base, (void *)(BANE + IMAGE_BASE_PHYSICAL), size);
      __ape_com_base = copied_base;
      __ape_com_size = size;
      KINFOF("%s @ %p,+%#zx", APE_COM_NAME, copied_base, size);
    }

    size = DIR_COUNT * sizeof(*__metal_dirs) + sizeof("/") + sizeof("/proc") +
      sizeof("/proc/self") + sizeof("/tmp");
    dm = sys_mmap_metal(NULL, size, PROT_READ | PROT_WRITE,
                        MAP_SHARED_linux | MAP_ANONYMOUS_linux, -1, 0);
    __metal_dirs = dm.addr;
    npassert(__metal_dirs != (void *)-1);

    char *strs = dm.addr + DIR_COUNT * sizeof(*__metal_dirs);
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

    // / -> /proc, /tmp, /zip
    __metal_dirs[0] = (struct MetalDirInfo){root_path, ROOT_INO, {
        {PROC_INO, 2, sizeof(*__metal_dirs), DT_DIR},
        { TMP_INO, 3, sizeof(*__metal_dirs), DT_DIR},
        { ZIP_INO, 4, sizeof(*__metal_dirs), DT_LNK}, EOD
    }};
    memcpy(__metal_dirs[0].ents[0].d_name, proc_name, sizeof("proc"));
    memcpy(__metal_dirs[0].ents[1].d_name, tmp_name, sizeof("tmp"));
    __metal_dirs[0].ents[2].d_name[0] = 'z';
    __metal_dirs[0].ents[2].d_name[1] = 'i';
    __metal_dirs[0].ents[2].d_name[2] = 'p';
    __metal_dirs[0].ents[2].d_name[3] = 0;

    // /proc -> /proc/self
    __metal_dirs[1] = (struct MetalDirInfo){proc_path, PROC_INO, {
        {PROC_SELF_INO, 2, sizeof(*__metal_dirs), DT_DIR}, EOD
    }};
    memcpy(__metal_dirs[1].ents[0].d_name, self_name, sizeof("self"));

    // /proc/self -> /proc/self/exec
    __metal_dirs[2] = (struct MetalDirInfo){proc_self_path, PROC_SELF_INO, {
        {PROC_SELF_EXE_INO, 2, sizeof(*__metal_dirs), DT_REG}, EOD
    }};
    __metal_dirs[2].ents[0].d_name[0] = 'e';
    __metal_dirs[2].ents[0].d_name[1] = 'x';
    __metal_dirs[2].ents[0].d_name[2] = 'e';
    __metal_dirs[2].ents[0].d_name[3] = 0;

    // /tmp
    __metal_dirs[3] = (struct MetalDirInfo){tmp_path, TMP_INO, {EOD}};

    __metal_dirs[4] = (struct MetalDirInfo){0};
  }
}

bool32 OpenMetalTmpFile(const char *file, struct MetalFile *state) {
  size_t idx = 0;
  size_t size;
  struct DirectMap dm;
  if (__metal_tmpfiles) {
    for (; __metal_tmpfiles[idx]; ++idx) {
      if (strcmp(file, __metal_tmpfiles[idx]) == 0) return false;
    }
  }

  if ((idx + 1) * sizeof(*__metal_tmpfiles) >= __metal_tmpfiles_size) {
    size = MAX(__metal_tmpfiles_size * __metal_tmpfiles_size, 4);
    dm = sys_mmap_metal(NULL, size, PROT_READ | PROT_WRITE,
                        MAP_SHARED_linux | MAP_ANONYMOUS_linux, -1, 0);
    npassert(dm.addr != (void *)-1);
    memcpy(dm.addr, __metal_tmpfiles, __metal_tmpfiles_size);
    sys_munmap_metal(__metal_tmpfiles, __metal_tmpfiles_size);
    __metal_tmpfiles = dm.addr;
    __metal_tmpfiles_size = size;
  }

  size = strlen(file) + 1;
  dm = sys_mmap_metal(NULL, size, PROT_READ | PROT_WRITE,
                      MAP_SHARED_linux | MAP_ANONYMOUS_linux, -1, 0);
  npassert(dm.addr != (void *)-1);
  memcpy(dm.addr, file, size);
  state->type = kMetalTmp;
  state->base = __metal_tmpfiles[idx] = dm.addr;
  state->size = size;
  return true;
}

bool32 CloseMetalTmpFile(struct MetalFile *state) {
  size_t idx = 0;
  for (; __metal_tmpfiles[idx]; ++idx) {
    if (strcmp(state->base, __metal_tmpfiles[idx]) == 0) break;
  }

  if ((idx + 1) * sizeof(*__metal_tmpfiles) == __metal_tmpfiles_size) {
    return false;
  }

  __metal_tmpfiles[idx] = 0;
  sys_munmap_metal(state->base, state->size);
  return true;
}

#endif /* __x86_64__ */
