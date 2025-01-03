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
#include "libc/inttypes.h"
#include "libc/irq/acpi.internal.h"

#ifdef __x86_64__

textstartup void _AcpiSratInit(void) {
  if (IsMetal()) {
    const AcpiTableSrat *srat;
    int cpu_count = 0;
    size_t length, nents = 0;
    const char *srat_end, *p;
    const AcpiSubtableHeader *h;
    const AcpiSrasLocalApic *apic;
    if (!_AcpiSuccess(_AcpiGetTable("SRAT", 0, (void **)&srat))) {
      KINFOF("no SRAT found");
      _AcpiCPUCount = -1;
      return;
    }
    length = srat->Header.Length;
    if (length < sizeof(*srat)) {
      KINFOF("SRAT has short length %#zx", length);
      _AcpiCPUCount = -1;
      return;
    }
    srat_end = (char *)srat + length;
    p = srat->Subtable;
    while (p != srat_end) {
      h = (const AcpiSubtableHeader *)p;
      ++nents;
      switch (h->Type) {
        case kAcpiSratLocalApic:
          apic = (const AcpiSrasLocalApic *)p;
          if (h->Length == sizeof(*apic) && (apic->Flags & 0x1) == 0x1) {
            ++cpu_count;
          }
      }
      p += h->Length;
    }
    KINFOF("SRAT @ %p, %#zx entries", srat, nents);
    if (cpu_count == 0) {
      KINFOF("no CPU SRAS entries in SRAT");
      cpu_count = -1;
    }
    _AcpiCPUCount = cpu_count;
  }
}

#endif /* __x86_64__ */
