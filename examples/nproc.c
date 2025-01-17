#if 0
/*─────────────────────────────────────────────────────────────────╗
│ To the extent possible under law, Justine Tunney has waived      │
│ all copyright and related or neighboring rights to this file,    │
│ as it is written in the following disclaimers:                   │
│   • http://unlicense.org/                                        │
│   • http://creativecommons.org/publicdomain/zero/1.0/            │
╚─────────────────────────────────────────────────────────────────*/
#endif
#include <cosmo.h>
#include <stdio.h>

// QEMU doesn't appear to support the SRAT ACPI table, so testing has
// been done in VMware Workstation which only supports VGA by default
__static_yoink("vga_console");

int main(int argc, char *argv[]) {
  printf("%d\n", __get_cpu_count());
}
