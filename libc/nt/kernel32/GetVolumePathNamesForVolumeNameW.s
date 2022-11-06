.include "o/libc/nt/codegen.inc"
.imp	kernel32,__imp_GetVolumePathNamesForVolumeNameW,GetVolumePathNamesForVolumeNameW,0

	.text.windows
GetVolumePathNamesForVolumeName:
	push	%rbp
	mov	%rsp,%rbp
	.profilable
	mov	__imp_GetVolumePathNamesForVolumeNameW(%rip),%rax
	jmp	__sysv2nt
	.endfn	GetVolumePathNamesForVolumeName,globl
	.previous
