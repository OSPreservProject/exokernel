TOP = ../..
PROG = as

GAS = $(TOP)/bin/gas

VPATH += $(GAS)
SRCFILES = app.c as.c atof-generic.c bignum-copy.c \
	cond.c expr.c flo-const.c flo-copy.c flonum-mult.c \
	frags.c hash.c hex-value.c input-file.c input-scrub.c \
	listing.c messages.c obstack.c output-file.c read.c subsegs.c \
	symbols.c version.c write.c xmalloc.c xrealloc.c
WARNOK = as.c cond.c expr.c messages.c read.c subsegs.c symbols.c write.c \

VPATH += $(GAS)/config
SRCFILES += tc-i386.c atof-ieee.c obj-aout.c

WARNOK += tc-i386.c atof-ieee.c

DEFS += -DNON_BROKEN_WORDS -DPIC -DOLD_GAS -DSIGTY=void -Derror=as_fatal \
	-DNETBSD_AOUT

export DOINSTALL=yes
export INSTALLPREFIX=usr/bin

EXTRAINC = -I. -I./config -I../../lib/libc
include $(TOP)/GNUmakefile.global
