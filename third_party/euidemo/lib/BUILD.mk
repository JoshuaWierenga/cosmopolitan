#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#── vi: set noet ft=make ts=8 sw=8 fenc=utf-8 :vi ────────────────────┘
#
# SYNOPSIS
#
#   Your package static library build config
#
# DESCRIPTION
#
#   Your library doesn't have a main() function and can be compromised
#   of sources written in multiple languages.
#
# WARNING
#
#   This library (third_party/euidemo/lib/) comes earlier in the
#   topological order of things than its parent (third_party/euidemo/)
#   because the parent package depends on the subpackage. Therefore,
#   this package needs to be written earlier in the Makefile includes.
#
# EXAMPLE
#
#   make o//third_party/euidemo/lib  # build library w/ sanity checks
#   ar t o//third_party/euidemo/lib/lib.a
#
# AUTHORS
#
#   Joshua Wierenga <joshuagit@joshuawierenga.tech>

PKGS += THIRD_PARTY_EUIDEMO_LIB

# Declares package i.e. library w/ transitive dependencies.
# We define packages as a thing that lumps similar sources.
# It's needed, so linkage can have a higher level overview.
# Mostly due to there being over 9,000 objects in the repo.
THIRD_PARTY_EUIDEMO_LIB =						\
	$(THIRD_PARTY_EUIDEMO_LIB_A_DEPS)				\
	$(THIRD_PARTY_EUIDEMO_LIB_A)

# Declares library w/o transitive dependencies.
THIRD_PARTY_EUIDEMO_LIB_A = o/$(MODE)/third_party/euidemo/lib/lib.a
THIRD_PARTY_EUIDEMO_LIB_ARTIFACTS += THIRD_PARTY_EUIDEMO_LIB_A

# Build configs might seem somewhat complicated. Rest assured they're
# mostly maintenance free. That's largely thanks to how we wildcard.
THIRD_PARTY_EUIDEMO_LIB_A_FILES := $(wildcard third_party/euidemo/lib/*)
THIRD_PARTY_EUIDEMO_LIB_A_HDRS = $(filter %.h,$(THIRD_PARTY_EUIDEMO_LIB_A_FILES))

# Define sets of source files without needing further iops.
THIRD_PARTY_EUIDEMO_LIB_A_SRCS_S =					\
	$(filter %.S,$(THIRD_PARTY_EUIDEMO_LIB_A_FILES))
THIRD_PARTY_EUIDEMO_LIB_A_SRCS_C =					\
	$(filter %.c,$(THIRD_PARTY_EUIDEMO_LIB_A_FILES))
THIRD_PARTY_EUIDEMO_LIB_A_SRCS =					\
	$(THIRD_PARTY_EUIDEMO_LIB_A_SRCS_S)			\
	$(THIRD_PARTY_EUIDEMO_LIB_A_SRCS_C)

# Change suffixes of different languages extensions into object names.
THIRD_PARTY_EUIDEMO_LIB_A_OBJS =					\
	$(THIRD_PARTY_EUIDEMO_LIB_A_SRCS_S:%.S=o/$(MODE)/%.o)	\
	$(THIRD_PARTY_EUIDEMO_LIB_A_SRCS_C:%.c=o/$(MODE)/%.o)

# Does the two most important things for C/C++ code sustainability.
# 1. Guarantees each header builds, i.e. includes symbols it needs.
# 2. Guarantees transitive closure of packages is directed acyclic.
THIRD_PARTY_EUIDEMO_LIB_A_CHECKS =					\
	$(THIRD_PARTY_EUIDEMO_LIB_A).pkg				\
	$(THIRD_PARTY_EUIDEMO_LIB_A_HDRS:%=o/$(MODE)/%.ok)

# Lists packages whose symbols are or may be directly referenced here.
# Note that linking stubs is always a good idea due to synthetic code.
THIRD_PARTY_EUIDEMO_LIB_A_DIRECTDEPS =				\
	LIBC_INTRIN						\
	LIBC_MEM						\
	LIBC_STDIO

# Evaluates variable as set of transitive package dependencies.
THIRD_PARTY_EUIDEMO_LIB_A_DEPS :=					\
	$(call uniq,$(foreach x,$(THIRD_PARTY_EUIDEMO_LIB_A_DIRECTDEPS),$($(x))))

# Concatenates object files into single file with symbol index.
# Please don't use fancy make features for mutating archives it's slow.
$(THIRD_PARTY_EUIDEMO_LIB_A):					\
		third_party/euidemo/lib/				\
		$(THIRD_PARTY_EUIDEMO_LIB_A).pkg			\
		$(THIRD_PARTY_EUIDEMO_LIB_A_OBJS)

# Asks packager to index symbols and validate their relationships.
# It's the real secret sauce for having a huge Makefile w/o chaos.
# @see tool/build/package.c
# @see build/rules.mk
$(THIRD_PARTY_EUIDEMO_LIB_A).pkg:					\
		$(THIRD_PARTY_EUIDEMO_LIB_A_OBJS)			\
		$(foreach x,$(THIRD_PARTY_EUIDEMO_LIB_A_DIRECTDEPS),$($(x)_A).pkg)

# Invalidates objects in package when makefile is edited.
$(THIRD_PARTY_EUIDEMO_LIB_A_OBJS): third_party/euidemo/lib/BUILD.mk

# let these assembly objects build on aarch64
o/$(MODE)/third_party/euidemo/lib/myasm.o: third_party/euidemo/lib/myasm.S
	@$(COMPILE) -AOBJECTIFY.S $(OBJECTIFY.S) $(OUTPUT_OPTION) -c $<

THIRD_PARTY_EUIDEMO_LIB_LIBS = $(foreach x,$(THIRD_PARTY_EUIDEMO_LIB_ARTIFACTS),$($(x)))
THIRD_PARTY_EUIDEMO_LIB_SRCS = $(foreach x,$(THIRD_PARTY_EUIDEMO_LIB_ARTIFACTS),$($(x)_SRCS))
THIRD_PARTY_EUIDEMO_LIB_HDRS = $(foreach x,$(THIRD_PARTY_EUIDEMO_LIB_ARTIFACTS),$($(x)_HDRS))
THIRD_PARTY_EUIDEMO_LIB_BINS = $(foreach x,$(THIRD_PARTY_EUIDEMO_LIB_ARTIFACTS),$($(x)_BINS))
THIRD_PARTY_EUIDEMO_LIB_CHECKS = $(foreach x,$(THIRD_PARTY_EUIDEMO_LIB_ARTIFACTS),$($(x)_CHECKS))
THIRD_PARTY_EUIDEMO_LIB_OBJS = $(foreach x,$(THIRD_PARTY_EUIDEMO_LIB_ARTIFACTS),$($(x)_OBJS))
THIRD_PARTY_EUIDEMO_LIB_TESTS = $(foreach x,$(THIRD_PARTY_EUIDEMO_LIB_ARTIFACTS),$($(x)_TESTS))

.PHONY: o/$(MODE)/third_party/euidemo/lib
o/$(MODE)/third_party/euidemo/lib: $(THIRD_PARTY_EUIDEMO_LIB_CHECKS)
