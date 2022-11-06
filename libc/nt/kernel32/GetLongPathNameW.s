.include "o/libc/nt/codegen.inc"
.imp	kernel32,__imp_GetLongPathNameW,GetLongPathNameW,0

	.text.windows
GetLongPathName:
	push	%rbp
	mov	%rsp,%rbp
	.profilable
	mov	__imp_GetLongPathNameW(%rip),%rax
	jmp	__sysv2nt
	.endfn	GetLongPathName,globl
	.previous
