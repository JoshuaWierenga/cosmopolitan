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
#include "libc/assert.h"
#include "libc/calls/metalfile.internal.h"
#include "libc/intrin/directmap.internal.h"
#include "libc/macros.internal.h"
#include "libc/str/str.h"
#include "libc/sysv/consts/prot.h"

#ifdef __x86_64__

struct MetalTmpFile *__metal_tmpfiles = NULL;
ptrdiff_t __metal_tmpfiles_max = 0;
size_t __metal_tmpfiles_size = 0;

// TODO: Support opening each file multiple times
// TODO: Support reopening files
// TODO: Reuse deleted spots in __metal_tmpfiles
bool32 _OpenMetalTmpFile(const char *path, struct MetalFile *file) {
  ptrdiff_t idx = 0;
  size_t size, buf_size;
  void *buf;

  if (__metal_tmpfiles) {
    for (; idx < __metal_tmpfiles_max; ++idx) {
      if (!__metal_tmpfiles[idx].deleted &&
          strcmp(path, __metal_tmpfiles[idx].name) == 0) {
        return false;
      }
    }
  }

  if ((idx + 1) * sizeof(*__metal_tmpfiles) >= __metal_tmpfiles_size) {
    size = MAX(__metal_tmpfiles_size << 1, 4 * sizeof(*__metal_tmpfiles));
    buf_size = _MetalAllocate(size, &buf);
    memcpy(buf, __metal_tmpfiles, __metal_tmpfiles_size);
    sys_munmap_metal(__metal_tmpfiles, __metal_tmpfiles_size);
    __metal_tmpfiles = buf;
    __metal_tmpfiles_size = buf_size;
  }

  size = strlen(path) + 1;
  buf_size = _MetalAllocate(size, &buf);
  memcpy(buf, path, size);
  __metal_tmpfiles[idx].deleted = false;
  __metal_tmpfiles[idx].name = buf;
  __metal_tmpfiles[idx].namesize = buf_size;
  __metal_tmpfiles[idx].filesize = 0;
  file->type = kMetalTmp;
  file->idx = idx;
  __metal_tmpfiles_max = MAX(__metal_tmpfiles_max, idx + 1);
  ++__metal_dirs[kMetalTmpDirIno].file_count;
  return true;
}

void _ExpandMetalTmpFile(struct MetalFile *file, const size_t min_size) {
  size_t size, buf_size;
  void *buf;
  if (__metal_tmpfiles[file->idx].filesize < min_size) {
    size = MAX(file->size << 1, min_size);
    buf_size = _MetalAllocate(size, &buf);
    memcpy(buf, file->base, file->size);
    sys_munmap_metal(file->base, file->size);
    file->base = buf;
    file->size = size;
  __metal_tmpfiles[file->idx].filesize = buf_size;
  } else {
    file->size = min_size;
  }
}

bool32 _CloseMetalTmpFile(struct MetalFile *file) {
  if (file->idx >= __metal_tmpfiles_max ||
      __metal_tmpfiles[file->idx].deleted) {
    return false;
  }
  if (file->base) {
    sys_munmap_metal(file->base, file->size);
  }
  sys_munmap_metal(__metal_tmpfiles[file->idx].name,
                   strlen(__metal_tmpfiles[file->idx].name) + 1);
  __metal_tmpfiles[file->idx].deleted = true;
  if (file->idx + 1 == __metal_tmpfiles_max) {
    while(__metal_tmpfiles_max > 0 &&
          __metal_tmpfiles[__metal_tmpfiles_max - 1].deleted) {
      --__metal_tmpfiles_max;
    }
  }
  --__metal_dirs[kMetalTmpDirIno].file_count;
  return true;
}

#endif /* __x86_64__ */
