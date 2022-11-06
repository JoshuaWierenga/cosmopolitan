.include "o/libc/nt/codegen.inc"
.imp	kernel32,__imp_GetVolumeInformationW,GetVolumeInformationW,0

	.text.windows
GetVolumeInformation:
	push	%rbp
	mov	%rsp,%rbp
	.profilable
	mov	__imp_GetVolumeInformationW(%rip),%rax
	jmp	__sysv2nt8
	.endfn	GetVolumeInformation,globl
	.previous
