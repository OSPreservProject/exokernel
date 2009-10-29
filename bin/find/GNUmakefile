TOP = ../..
PROG = find
SRCFILES = find.c function.c ls.c main.c misc.c operator.c option.c
DEFS=-Dlint
UNINITOK = $(SRCFILES)
UNUSEDOK = $(SRCFILES)
BADFORMATOK = function.c misc.c
WARNOK = main.c

export DOINSTALL=yes

EXTRAINC = -I../../lib/libc
include $(TOP)/GNUmakefile.global
