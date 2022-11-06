.include "o/libc/nt/codegen.inc"
.imp	kernel32,__imp_GetVolumeNameForVolumeMountPointW,GetVolumeNameForVolumeMountPointW,0

	.text.windows
GetVolumeNameForVolumeMountPoint:
	push	%rbp
	mov	%rsp,%rbp
	.profilable
	mov	__imp_GetVolumeNameForVolumeMountPointW(%rip),%rax
	jmp	__sysv2nt
	.endfn	GetVolumeNameForVolumeMountPoint,globl
	.previous
