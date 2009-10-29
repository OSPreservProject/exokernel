TOP = ../..
PROG = agrep
SRCFILES = asearch.c checkfile.c main.c parse.c utilities.c \
	asearch1.c      compat.c        maskgen.c       preprocess.c \
	bitap.c         follow.c        mgrep.c         sgrep.c

DEFS=

export DOINSTALL=yes
export INSTALLPREFIX=usr/bin

EXTRAINC = -I../../lib/libc -I../../sys

include $(TOP)/GNUmakefile.global

# I really don't want to ansify this code....  ;-)
CFLAGS := $(filter-out -Werror -Wall, $(CFLAGS))

