#ifndef COSMOPOLITAN_LIBC_CALLS_METALFILE_INTERNAL_H_
#define COSMOPOLITAN_LIBC_CALLS_METALFILE_INTERNAL_H_
#if !(__ASSEMBLER__ + __LINKER__ + 0)
#include "libc/calls/struct/dirent.h"

COSMOPOLITAN_C_START_

#define kMetalBad  0 // used if file does not exist
#define kMetalApe  1
#define kMetalDir  2
#define kMetalTmp  3
#define kMetalFile 4

#define kMetalDirCount     6 // __metal_dirs length i.e. number of hardcoded metal dirs
#define kMetalDirStaticMax 4 // __metal_tmpfiles[i].ents length i.e. maximum child count for each metal dir
#define kMetalFileCount    2 // __metal_files length i.e. number of hardcoded metal files

/* metal has 3 simulated filesystems:
/:    read only, contains /dev/null, /dev/zero and /proc/self/exe(managed seperately to __metal_files)
/tmp: read write
/zip: read only, zipos
*/

// Inos:

// Directories:
// 0: /
// 1: /dev
// 2: /proc
// 3: /tmp
// 4: /zip
// 5: /proc/self

// Files:
// 6: /dev/null
// 7: /dev/zero

// Specially managed file
// 8: /proc/self/exe

// Tmp:
// 9+: /tmp/*

// Other defines are in metalfile.c
#define kMetalTmpDirIno      3
#define kMetalProcSelfExeIno 8
#define kMetalTmpInoStart kMetalDirCount + kMetalFileCount + 1

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
struct MetalDir {
  const char *path;
  ptrdiff_t file_count; // total number of files, for kMetalTmp this can be different from ent_count
  ptrdiff_t ent_count;  // number of files in ents
  struct dirent ents[kMetalDirStaticMax];
};

// TODO(joshua): Store copies of MetalFile's base and size
struct MetalTmpFile {
  char *name;
  int64_t namesize; // allocated size of name
  int64_t filesize; // allocated size of file, can be larger than MetalFile's size
  bool32 deleted;
};

struct MetalFile {
  uint8_t type;
  uint32_t mode;    // only set if !kMetalBad
  ptrdiff_t idx;    // ino/__metal_dirs/__metal_files(-kMetalDirCount) index for kMetalDir/kMetalFile, __metal_tmpfiles index for kMetalTmp
  const char *path; // only set for kMetalFile
  int64_t size;     // only set for kMetalFile
};

// TODO(joshua): Support kFdDevNull?
struct MetalOpenFile {
  uint8_t type;
  uint8_t *base; // base address of file for kMetalApe and kMetalTmp
  size_t size;   // size of file at base for kMetalApe and kMetalTmp
  size_t pos;    // byte offset from base for kMetalApe and kMetalTmp, tell for kMetalDir
  ptrdiff_t idx; // ino/__metal_dirs index for kMetalDir, __metal_tmpfiles index for kMetalTmp
};

extern void *__ape_com_base;
extern size_t __ape_com_size;
extern uint16_t __ape_com_sectors;  // ape/ape.S

extern struct MetalDir *__metal_dirs;
extern struct MetalFile *__metal_files;

extern struct MetalTmpFile *__metal_tmpfiles;
extern ptrdiff_t __metal_tmpfiles_max;
extern size_t __metal_tmpfiles_size;

extern ptrdiff_t __metal_cwd_ino;

int sys_chdir_metal(const char *path);
int sys_fchdir_metal(int dirfd);

char *_MetalPath(const char *filename, char *resolved);
// This function returns static memory so don't keep the result around
char *_MetalFullPath(int dirfd, const char *path);

size_t _MetalAllocate(size_t size, void **addr);

// Do not call directly, use the openat, write and close functions and their wrappers
bool32 _OpenMetalTmpFile(const char *path, struct MetalOpenFile *file);
void _ExpandMetalTmpFile(struct MetalOpenFile *file, const size_t min_size);
bool32 _CloseMetalTmpFile(struct MetalOpenFile *file);

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */

#define APE_COM_NAME "/proc/self/exe"

#endif /* COSMOPOLITAN_LIBC_CALLS_METALFILE_INTERNAL_H_ */
