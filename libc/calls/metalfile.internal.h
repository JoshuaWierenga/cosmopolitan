#ifndef COSMOPOLITAN_LIBC_CALLS_METALFILE_INTERNAL_H_
#define COSMOPOLITAN_LIBC_CALLS_METALFILE_INTERNAL_H_
#if !(__ASSEMBLER__ + __LINKER__ + 0)
#include "libc/calls/struct/dirent.h"

COSMOPOLITAN_C_START_

#define kMetalApe  0
#define kMetalDir  1

struct MetalDirInfo {
  char *path;
  uint64_t ino;
  struct dirent ents[3];
};

struct MetalFile {
  char type;
  char *base;
  size_t size;
  size_t pos;
};

extern void *__ape_com_base;
extern size_t __ape_com_size;
extern uint16_t __ape_com_sectors;  // ape/ape.S
extern struct MetalDirInfo *__metal_dirs;

COSMOPOLITAN_C_END_
#endif /* !(__ASSEMBLER__ + __LINKER__ + 0) */

#define APE_COM_NAME "/proc/self/exe"

#endif /* COSMOPOLITAN_LIBC_CALLS_METALFILE_INTERNAL_H_ */
