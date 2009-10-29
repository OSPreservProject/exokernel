
TOP = ../..
PROG = ar
SRCFILES = append.c ar.c archive.c contents.c delete.c extract.c misc.c \
        move.c print.c replace.c 
DEFS = -Dlint
UNINITOK = ar.c archive.c
WARNOK = archive.c
BADFORMATOK = ar.c

export DOINSTALL=yes
export INSTALLPREFIX=usr/bin

EXTRAINC = -I../../lib/libc -I.
include $(TOP)/GNUmakefile.global
