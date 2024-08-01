#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#── vi: set noet ft=make ts=8 sw=8 fenc=utf-8 :vi ────────────────────┘
#
# SYNOPSIS
#
#   Your package build config for executable programs
#
# DESCRIPTION
#
#   We assume each .c file in this directory has a main() function, so
#   that it becomes as easy as possible to write lots of tiny programs
#
# EXAMPLE
#
#   make o//third_party/euidemo
#   o/third_party/euidemo/program.com
#
# AUTHORS
#
#   Joshua Wierenga <joshuagit@joshuawierenga.tech>

PKGS += THIRD_PARTY_EUIDEMO

# Reads into memory the list of files in this directory.
THIRD_PARTY_EUIDEMO_FILES := $(wildcard third_party/euidemo/*)

# Defines sets of files without needing further iops.
THIRD_PARTY_EUIDEMO_SRCS = $(filter %.c,$(THIRD_PARTY_EUIDEMO_FILES))
THIRD_PARTY_EUIDEMO_HDRS = $(filter %.h,$(THIRD_PARTY_EUIDEMO_FILES))
THIRD_PARTY_EUIDEMO_COMS = $(THIRD_PARTY_EUIDEMO_SRCS:%.c=o/$(MODE)/%.com)
THIRD_PARTY_EUIDEMO_BINS =					\
	$(THIRD_PARTY_EUIDEMO_COMS)			\
	$(THIRD_PARTY_EUIDEMO_COMS:%=%.dbg)

# Remaps source file names to object names.
# Also asks a wildcard rule to automatically run tool/build/zipobj.c
THIRD_PARTY_EUIDEMO_OBJS =					\
	$(THIRD_PARTY_EUIDEMO_SRCS:%.c=o/$(MODE)/%.o)

# Lists packages whose symbols are or may be directly referenced here.
# Note that linking stubs is always a good idea due to synthetic code.
THIRD_PARTY_EUIDEMO_DIRECTDEPS =				\
	LIBC_INTRIN						\
	LIBC_MEM						\
	LIBC_NEXGEN32E						\
	LIBC_NT_GDI32						\
	LIBC_NT_KERNEL32					\
	LIBC_NT_USER32						\
	LIBC_STDIO						\
	LIBC_X						\
	LIBC_VGA						\
	THIRD_PARTY_EUIDEMO_LIB

# Evaluates the set of transitive package dependencies.
THIRD_PARTY_EUIDEMO_DEPS :=				\
	$(call uniq,$(foreach x,$(THIRD_PARTY_EUIDEMO_DIRECTDEPS),$($(x))))

$(THIRD_PARTY_EUIDEMO_A).pkg:				\
		$(THIRD_PARTY_EUIDEMO_OBJS)		\
		$(foreach x,$(THIRD_PARTY_EUIDEMO_DIRECTDEPS),$($(x)_A).pkg)

# Specifies how to build programs as ELF binaries with DWARF debug info.
# @see build/rules.mk for definition of rule that does .com.dbg -> .com
o/$(MODE)/third_party/euidemo/%.com.dbg:			\
		$(THIRD_PARTY_EUIDEMO_DEPS)		\
		o/$(MODE)/third_party/euidemo/%.o		\
		$(CRT)					\
		$(APE_NO_MODIFY_SELF)
	@$(APELINK)

# Invalidates objects in package when makefile is edited.
$(THIRD_PARTY_EUIDEMO_OBJS): third_party/euidemo/BUILD.mk

# Creates target building everything in package and subpackages.
.PHONY: o/$(MODE)/third_party/euidemo
o/$(MODE)/third_party/euidemo:				\
		o/$(MODE)/third_party/euidemo/lib		\
		$(THIRD_PARTY_EUIDEMO_BINS)
