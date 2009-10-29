TOP = ../..
PROG = mv
SRCFILES = mv.c
DEFS=-Dlint
UNINITOK = mv.c
UNUSEDOK = mv.c
BADFORMATOK = mv.c

export DOINSTALL=yes

EXTRAINC = -I$(TOP)/lib/libc
EXTRAOBJ = $(TOP)/lib/libutil/obj/libutil.a
include $(TOP)/GNUmakefile.global
