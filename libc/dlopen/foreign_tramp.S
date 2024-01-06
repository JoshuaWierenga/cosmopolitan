/*-*- mode:unix-assembly; indent-tabs-mode:t; tab-width:8; coding:utf-8     -*-│
│ vi: set et ft=asm ts=8 tw=8 fenc=utf-8                                   :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2024 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/macros.internal.h"

#define SIZE 0x0200
#define SKEW 0x10

#ifdef __x86_64__
// Wrapper for dlsym loaded function to ensure all passed
// parameters actually make it.
//
// @params all x86_64 sysv integer parameter registers and
// 4 float/vector registers + any on stack
// This wrapper is unlikely to work for structs passed by value
// @param r11 is pointer to dlsym loaded function
// @return dlsym loaded function's return value in rax or xmm0
foreign_tramp_nt:
  popq   %r10 # return pointer
  pushq  %r9  # integer/pointer argument 6
  pushq  %r8  # integer/pointer argument 5
  pushq  %r10
  pushq  %rbp
  mov    %rsp, %rbp
  pushq  %r11 # windows function pointer
  pushq  %rdi # integer/pointer argument 1
  pushq  %rsi # integer/pointer argument 2
  pushq  %rdx # integer/pointer argument 3
  pushq  %rcx # integer/pointer argument 4
  sub    $72, %rsp
  # TODO: Support vectorcall
  movdqa %xmm0, (%rsp)   # floating point argument 1
  movdqa %xmm1, 16(%rsp) # floating point argument 2
  movdqa %xmm2, 32(%rsp) # floating point argument 3
  movdqa %xmm3, 48(%rsp) # floating point argument 4
  # 3 pointer array: %rbx, %rbp, return pointer to caller from stack
  movq   $24, %rdi
  call   malloc
  movq   %rbx, (%rax)
  movq   %rbp, 8(%rax)
  movq   %rax, %rbx
  movdqa 48(%rsp), %xmm3
  movdqa 32(%rsp), %xmm2
  movdqa 16(%rsp), %xmm1
  movdqa (%rsp), %xmm0
  add    $72, %rsp
  popq   %r9
  popq   %r8
  popq   %rdx
  popq   %rcx
  popq   %r11
  popq   %rbp
  popq   16(%rbx)

  sub    $32, %rsp
  call   *%r11
  add    $32, %rsp
  popq   %r8
  popq   %r9

  pushq  16(%rbx)
  pushq  %rbp
  movq   8(%rbx), %rbp
  pushq  %rax # integer/pointer return
  sub    $24, %rsp
  movdqa %xmm0, (%rsp) # floating point return
  mov    %rbx, %rdi
  movq   (%rdi), %rbx
  call   free
  movdqa (%rsp), %xmm0
  add    $24, %rsp
  popq   %rax
  popq   %rbp
  ret

	.endfn	foreign_tramp_nt,globl

#endif /* __x86_64__ */

foreign_tramp_sysv:

#ifdef __x86_64__

// Wrapper for dlsym loaded function to block signals, switch tls
// and ensure all passed parameters actually make it.
//
// @params all x86_64 sysv parameter registers + any on stack
// @param r11  is pointer to dlsym loaded function
// @return dlsym loaded function's return value

  pushq  %rbp
  mov    %rsp, %rbp
  pushq  %r11
  pushq  %rax # vararg vector register count
  pushq  %rdi # integer/pointer argument 1
  pushq  %rsi # integer/pointer argument 2
  pushq  %rdx # integer/pointer argument 3
  pushq  %rcx # integer/pointer argument 4
  pushq  %r8  # integer/pointer argument 5
  pushq  %r9  # integer/pointer argument 6
  # TODO(joshua): Check if ymm and zmm are used
  sub    $128, %rsp
  movdqa %xmm0, (%rsp)    # floating point argument 1
  movdqa %xmm1, 16(%rsp)  # floating point argument 2
  movdqa %xmm2, 32(%rsp)  # floating point argument 3
  movdqa %xmm3, 48(%rsp)  # floating point argument 4
  movdqa %xmm4, 64(%rsp)  # floating point argument 5
  movdqa %xmm5, 80(%rsp)  # floating point argument 6
  movdqa %xmm6, 96(%rsp)  # floating point argument 7
  movdqa %xmm7, 112(%rsp) # floating point argument 8
  # TODO: Move malloc/free into setup and restore functions?
  # TODO: Avoid continual malloc/free by mallocing once and reusing?
  # 3 pointer array: %rbx, %rbp(with original %rsp in it), return pointer to caller from stack
  movq   $24, %rdi
  call   malloc
  movq   %rbx, (%rax)
  movq   %rbp, 8(%rax)
  movq   %rax, %rbx
  call   foreign_tramp_setup
  movdqa 112(%rsp), %xmm7
  movdqa 96(%rsp), %xmm6
  movdqa 80(%rsp), %xmm5
  movdqa 64(%rsp), %xmm4
  movdqa 48(%rsp), %xmm3
  movdqa 32(%rsp), %xmm2
  movdqa 16(%rsp), %xmm1
  movdqa (%rsp), %xmm0
  add    $128, %rsp
  popq   %r9
  popq   %r8
  popq   %rcx
  popq   %rdx
  popq   %rsi
  popq   %rdi
  popq   %rax
  popq   %r11
  popq   %rbp
  popq   16(%rbx)

  # At this point everything is the same as it was right before this function
  # was called except that %rbx contains a 3 pointer array and so we can call
  # the requested function and have it be able to read of its all parameters.
  call   *%r11

  pushq  16(%rbx)
  pushq  %rbp
  movq   8(%rbx), %rbp
  # TODO(joshua): Confirm that c/c++ can actually use the second return registers in each case
  # I just followed Figure 3.4 from the sysv x86_64 abi
  pushq  %rax # integer/pointer return 1
  pushq  %rdx # integer/pointer return 2
  sub    $64, %rsp
  # TODO(joshua): Check if ymm and zmm are used
  movdqa %xmm0, (%rsp)   # floating point return 1
  movdqa %xmm1, 16(%rsp) # floating point return 2
  fstpt  32(%rsp) # long double return 1
  fstpt  48(%rsp) # long double return 2
  call   foreign_tramp_restore
  mov    %rbx, %rdi
  movq   (%rdi), %rbx
  call   free
  fldt   48(%rsp)
  fldt   32(%rsp)
  movdqa 16(%rsp), %xmm1
  movdqa (%rsp), %xmm0
  add    $64, %rsp
  popq   %rdx
  popq   %rax
  popq   %rbp
  ret

#elif defined(__aarch64__)

	stp	x29,x30,[sp,-0x100]!
	mov	x29,sp

//	save vector arguments
	stp	q0,q1,[sp,0x10]
	stp	q2,q3,[sp,0x30]
	stp	q4,q5,[sp,0x50]
	stp	q6,q7,[sp,0x70]

//	save register arguments
	stp	x0,x1,[sp,0x90]
	stp	x2,x3,[sp,0xa0]
	stp	x4,x5,[sp,0xb0]
	stp	x6,x7,[sp,0xc0]

//	save function pointer
	str	x8,[sp,0xd0]

//	block signals
	bl	__sig_block
	str	x0,[sp,0xd8]

//	move stack arguments
	sub	sp,sp,#0x100
	ldp	q0,q1,[sp,SIZE+0x00]
	ldp	q2,q3,[sp,SIZE+0x20]
	ldp	q4,q5,[sp,SIZE+0x40]
	ldp	q6,q7,[sp,SIZE+0x60]
	stp	q0,q1,[sp,0x00]
	stp	q2,q3,[sp,0x20]
	stp	q4,q5,[sp,0x40]
	stp	q6,q7,[sp,0x60]
	ldp	q0,q1,[sp,SIZE+0x80]
	ldp	q2,q3,[sp,SIZE+0xa0]
	ldp	q4,q5,[sp,SIZE+0xc0]
	ldp	q6,q7,[sp,SIZE+0xe0]
	stp	q0,q1,[sp,0x80]
	stp	q2,q3,[sp,0xa0]
	stp	q4,q5,[sp,0xc0]
	stp	q6,q7,[sp,0xe0]

//	restore vector arguments
	ldp	q0,q1,[sp,0x100+0x10]
	ldp	q2,q3,[sp,0x100+0x30]
	ldp	q4,q5,[sp,0x100+0x50]
	ldp	q6,q7,[sp,0x100+0x70]

//	restore register arguments
	ldp	x0,x1,[sp,0x100+0x90]
	ldp	x2,x3,[sp,0x100+0xa0]
	ldp	x4,x5,[sp,0x100+0xb0]
	ldp	x6,x7,[sp,0x100+0xc0]

//	call function
	ldr	x8,[sp,0x100+0xd0]
	blr	x8
	add	sp,sp,#0x100

//	save vector results
	stp	q0,q1,[sp,0x10]
	stp	q2,q3,[sp,0x30]
	stp	q4,q5,[sp,0x50]
	stp	q6,q7,[sp,0x70]

//	save register results
	stp	x0,x1,[sp,0x90]
	stp	x2,x3,[sp,0xa0]
	stp	x4,x5,[sp,0xb0]
	stp	x6,x7,[sp,0xc0]

//	unblock signals
	ldr	x0,[sp,0xd8]
	bl	__sig_unblock

//	restore vector results
	ldp	q0,q1,[sp,0x10]
	ldp	q2,q3,[sp,0x30]
	ldp	q4,q5,[sp,0x50]
	ldp	q6,q7,[sp,0x70]

//	restore register results
	ldp	x0,x1,[sp,0x90]
	ldp	x2,x3,[sp,0xa0]
	ldp	x4,x5,[sp,0xb0]
	ldp	x6,x7,[sp,0xc0]

	ldp	x29,x30,[sp],0x100
	ret

#endif // __x86_64__

	.endfn	foreign_tramp_sysv,globl
