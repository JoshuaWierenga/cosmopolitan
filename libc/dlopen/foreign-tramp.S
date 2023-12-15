#include "libc/macros.internal.h"
.privileged

// Wrapper for dlsym loaded function to block signals, switch tls
// and ensure all passed parameters actually make it.
//
// @params all x86_64 sysv parameter registers + any on stack
// @param r11  is pointer to dlsym loaded function
// @return dlsym loaded function's return value

#ifdef __x86_64__
foreign_tramp:
  pushq  %r11
  pushq  %rbp
  mov    %rsp, %rbp
  pushq  %rax # vararg vector register count
  pushq  %rdi # integer/pointer argument 1
  pushq  %rsi # integer/pointer argument 2
  pushq  %rdx # integer/pointer argument 3
  pushq  %rcx # integer/pointer argument 4
  pushq  %r8  # integer/pointer argument 5
  pushq  %r9  # integer/pointer argument 6
  # TODO(joshua): Check if ymm and zmm are used
  sub    $144, %rsp
  movdqa %xmm0, (%rsp)     # floating point argument 1
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
  movq   $malloc, %r10
  movq   $24, %rdi
  call   *%r10
  movq   %rbx, (%rax)
  movq   %rbp, 8(%rax)
  movq   %rax, %rbx
  movq   $foreign_tramp_setup, %r10
  call   *%r10
  movdqa 112(%rsp), %xmm7
  movdqa 96(%rsp), %xmm6
  movdqa 80(%rsp), %xmm5
  movdqa 64(%rsp), %xmm4
  movdqa 48(%rsp), %xmm3
  movdqa 32(%rsp), %xmm2
  movdqa 16(%rsp), %xmm1
  movdqa (%rsp), %xmm0
  add    $144, %rsp
  popq   %r9
  popq   %r8
  popq   %rcx
  popq   %rdx
  popq   %rsi
  popq   %rdi
  popq   %rax
  popq   %rbp
  popq   %r11
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
  pushq  %rax  # integer/pointer return 1
  pushq  %rdx  # integer/pointer return 2
  sub    $64, %rsp
  # TODO(joshua): Check if ymm and zmm are used
  movdqa %xmm0, (%rsp)     # floating point return 1
  movdqa %xmm1, 16(%rsp)  # floating point return 2
  fstpt  32(%rsp)  # long double return 1
  fstpt  48(%rsp)  # long double return 2
  movq   $foreign_tramp_restore, %r10
  call   *%r10
  movq   $free, %r10
  mov    %rbx, %rdi
  movq   (%rdi), %rbx
  call   *%r10
  fldt   48(%rsp)
  fldt   32(%rsp)
  movdqa 16(%rsp), %xmm1
  movdqa (%rsp), %xmm0
  add    $64, %rsp
  popq   %rdx
  popq   %rax
  popq   %rbp
  ret

	.endfn	foreign_tramp,globl,hidden
#endif /* __x86_64__ */
