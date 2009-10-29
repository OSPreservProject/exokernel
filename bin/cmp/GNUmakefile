
TOP = ../..
PROG = cmp
SRCFILES = cmp.c misc.c regular.c special.c
UNINITOK += special.c
UNUSEDOK += cmp.c
DEFS += -Dlint


export DOINSTALL=yes
INSTALLPREFIX=usr/bin

EXTRAINC = -I../../lib/libc
include $(TOP)/GNUmakefile.global
