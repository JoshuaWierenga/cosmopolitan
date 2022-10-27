#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

PKGS += THIRD_PARTY_WIN_POLYFILL

THIRD_PARTY_WIN_POLYFILL =						\
	$(THIRD_PARTY_WIN_POLYFILL_A_DEPS)			\
	$(THIRD_PARTY_WIN_POLYFILL_A)

THIRD_PARTY_WIN_POLYFILL_A = o/$(MODE)/third_party/win-polyfill/win-polyfill.a
THIRD_PARTY_WIN_POLYFILL_ARTIFACTS += THIRD_PARTY_WIN_POLYFILL_A

THIRD_PARTY_WIN_POLYFILL_A_FILES :=					\
    $(wildcard third_party/win-polyfill/src/*)
    
THIRD_PARTY_WIN_POLYFILL_A_HDRS_h =					\
	$(filter %.h,$(THIRD_PARTY_WIN_POLYFILL_A_FILES))
THIRD_PARTY_WIN_POLYFILL_A_HDRS_hpp =				\
	$(filter %.hpp,$(THIRD_PARTY_WIN_POLYFILL_A_FILES))
THIRD_PARTY_WIN_POLYFILL_A_HDRS =					\
	$(THIRD_PARTY_WIN_POLYFILL_A_HDRS_h)			\
	$(THIRD_PARTY_WIN_POLYFILL_A_HDRS_hpp)

THIRD_PARTY_WIN_POLYFILL_A_SRCS =					\
	$(filter %.c,$(THIRD_PARTY_WIN_POLYFILL_A_FILES))

THIRD_PARTY_WIN_POLYFILL_A_OBJS =					\
	$(THIRD_PARTY_WIN_POLYFILL_A_SRCS:%.c=o/$(MODE)/%.o)

THIRD_PARTY_WIN_POLYFILL_A_CHECKS =					\
	$(THIRD_PARTY_WIN_POLYFILL_A).pkg				\
	$(THIRD_PARTY_WIN_POLYFILL_A_HDRS:%=o/$(MODE)/%.ok)

THIRD_PARTY_WIN_POLYFILL_A_DIRECTDEPS =             \
	LIBC_INTRIN                                     \
	LIBC_NEXGEN32E                                  \
	LIBC_NT_NTDLL                                   \
	LIBC_NT_KERNEL32                                \
	LIBC_STR


THIRD_PARTY_WIN_POLYFILL_A_DEPS :=					\
	$(call uniq,$(foreach x,$(THIRD_PARTY_WIN_POLYFILL_A_DIRECTDEPS),$($(x))))

$(THIRD_PARTY_WIN_POLYFILL_A):					\
		third_party/win-polyfill/				\
		$(THIRD_PARTY_WIN_POLYFILL_A).pkg			\
		$(THIRD_PARTY_WIN_POLYFILL_A_OBJS)

$(THIRD_PARTY_WIN_POLYFILL_A).pkg:					\
		$(THIRD_PARTY_WIN_POLYFILL_A_OBJS)			\
		$(foreach x,$(THIRD_PARTY_WIN_POLYFILL_A_DIRECTDEPS),$($(x)_A).pkg)

$(THIRD_PARTY_WIN_POLYFILL_A_OBJS): third_party/win-polyfill/win-polyfill.mk

THIRD_PARTY_WIN_POLYFILL_LIBS = $(foreach x,$(THIRD_PARTY_WIN_POLYFILL_ARTIFACTS),$($(x)))
THIRD_PARTY_WIN_POLYFILL_SRCS = $(foreach x,$(THIRD_PARTY_WIN_POLYFILL_ARTIFACTS),$($(x)_SRCS))
THIRD_PARTY_WIN_POLYFILL_HDRS = $(foreach x,$(THIRD_PARTY_WIN_POLYFILL_ARTIFACTS),$($(x)_HDRS))
THIRD_PARTY_WIN_POLYFILL_BINS = $(foreach x,$(THIRD_PARTY_WIN_POLYFILL_ARTIFACTS),$($(x)_BINS))
THIRD_PARTY_WIN_POLYFILL_CHECKS = $(foreach x,$(THIRD_PARTY_WIN_POLYFILL_ARTIFACTS),$($(x)_CHECKS))
THIRD_PARTY_WIN_POLYFILL_OBJS = $(foreach x,$(THIRD_PARTY_WIN_POLYFILL_ARTIFACTS),$($(x)_OBJS))
THIRD_PARTY_WIN_POLYFILL_TESTS = $(foreach x,$(THIRD_PARTY_WIN_POLYFILL_ARTIFACTS),$($(x)_TESTS))

.PHONY: o/$(MODE)/third_party/win-polyfill
o/$(MODE)/third_party/win-polyfill: $(THIRD_PARTY_WIN_POLYFILL_CHECKS)
