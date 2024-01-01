#ifndef COSMOPOLITAN_LIBC_CALLS_METALFILE_INTERNAL_H_
#define COSMOPOLITAN_LIBC_CALLS_METALFILE_INTERNAL_H_
#if !(__ASSEMBLER__ + __LINKER__ + 0)
#include "libc/calls/struct/dirent.h"

COSMOPOLITAN_C_START_

#define kMetalApe  0
#define kMetalDir  1
#define kMetalTmp  2

#define kMetalDirCount     5 // __metal_tmpfiles element count
#define kMetalDirStaticMax 4 // __metal_tmpfiles[i].ents max element count

#define kMetalTmpDirIno   2
#define kMetalTmpInoStart 6

/* Example code to iterate over files
  for (ptrdiff_t ino = 0; ino < kMetalDirCount; ++ino) {
    if (!__metal_dirs[ino].path) continue;
    printf("files in %s:\n", __metal_dirs[ino].path);
    if (ino == kMetalTmpDirIno) {
      for (ptrdiff_t off = 0; off < __metal_tmpfiles_max; ++off) {
        if (!__metal_tmpfiles[off]) continue;
        printf("%s\n", __metal_tmpfiles[off]);
      }
    } else {
      for (ptrdiff_t off = 0; off < __metal_dirs[ino].ent_count; ++off) {
        printf("%s\n", __metal_dirs[ino].ents[off].d_name);
      }
    }
  }

  Example code to iterate over inos, assuming a tree:
  bool32 root;
  puts("ino, path");
  for (ptrdiff_t ino = 0; ino < kMetalDirCount; ++ino) {
    if (!__metal_dirs[ino].path) continue;
    if ((root = strcmp(__metal_dirs[ino].path, "/") == 0)) {
      printf("%3td, %s\n", ino, __metal_dirs[ino].path);
    }
    for (ptrdiff_t off = 0; off < __metal_dirs[ino].ent_count; ++off) {
      if (root) {
        printf("%3td, /%s\n", __metal_dirs[ino].ents[off].d_ino,
               __metal_dirs[ino].ents[off].d_name);
      } else {
        printf("%3td, %s/%s\n", __metal_dirs[ino].ents[off].d_ino,
               __metal_dirs[ino].path, __metal_dirs[ino].ents[off].d_name);
      }
    }
  }
  for (ptrdiff_t off = 0; off < __metal_tmpfiles_max; ++off) {
    if (!__metal_tmpfiles[off]) continue;
    printf("%3td, %s/%s\n", kMetalTmpInoStart + off,
           __metal_dirs[kMetalTmpDirIno].path, __metal_tmpfiles[off]);
  }
*/

// TODO(joshua): Support directories within /tmp? If so only store name here and use recursion to construct full path
// TODO(joshua): Use or remove file_count
struct MetalDirInfo {
  char *path;
  ptrdiff_t file_count; // total number of files, for kMetalTmp this can be different from ent_count
  ptrdiff_t ent_count;  // number of files in ents
  struct dirent ents[kMetalDirStaticMax];
};

// TODO(joshua): Support kFdDevNull?
struct MetalFile {
  char type;
  char *base;
  size_t size;   // size of base for kMetalApe and kMetalTmp
  size_t pos;    // offset from base for kMetalApe and kMetalTmp, tell for kMetalDir
  ptrdiff_t idx; // ino/__metal_dirs index for kMetalDir, __metal_tmpfiles index for kMetalTmp
};

extern void *__ape_com_base;
extern size_t __ape_com_size;
extern uint16_t __ape_com_sectors;  // ape/ape.S

extern struct MetalDirInfo *__metal_dirs;

extern char **__metal_tmpfiles;
extern ptrdiff_t __metal_tmpfiles_max;
extern size_t __metal_tmpfiles_size;

extern ptrdiff_t __metal_cwd_ino;

int sys_chdir_metal(const char *path);
int sys_fchdir_metal(int dirfd);

// Do not call directly, use the openat, write and close functions and their wrappers
bool32 OpenMetalTmpFile(const char *path, struct MetalFile *file);
void ResizeMetalTmpFile(struct MetalFile *file, const size_t min_size);
bool32 CloseMetalTmpFile(struct MetalFile *file);

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */

#define APE_COM_NAME "/proc/self/exe"

#endif /* COSMOPOLITAN_LIBC_CALLS_METALFILE_INTERNAL_H_ */
