#ifndef COSMOPOLITAN_LIBC_CALLS_METALFILE_INTERNAL_H_
#define COSMOPOLITAN_LIBC_CALLS_METALFILE_INTERNAL_H_
#if !(__ASSEMBLER__ + __LINKER__ + 0)
#include "libc/calls/struct/dirent.h"

COSMOPOLITAN_C_START_

#define kMetalApe  0
#define kMetalDir  1
#define kMetalTmp  2

#define TMP_INO_START 6

struct MetalDirInfo {
  char *path;
  uint64_t ino;
  struct dirent ents[4];
};

struct MetalFile {
  char type;
  char *base;
  size_t size;
  size_t pos;
  ptrdiff_t idx;
};

extern void *__ape_com_base;
extern size_t __ape_com_size;
extern uint16_t __ape_com_sectors;  // ape/ape.S
extern struct MetalDirInfo *__metal_dirs;
extern char **__metal_tmpfiles;
extern size_t __metal_tmpfiles_max;
extern size_t __metal_tmpfiles_size;

// Do not call directly, use openat, writev and close
bool32 OpenMetalTmpFile(const char *file, struct MetalFile *state);
void ResizeMetalTmpFile(struct MetalFile *file, const size_t min_size);
bool32 CloseMetalTmpFile(struct MetalFile *state);

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */

#define APE_COM_NAME "/proc/self/exe"

#endif /* COSMOPOLITAN_LIBC_CALLS_METALFILE_INTERNAL_H_ */
