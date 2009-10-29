TOP = ../..
PROG = memdump
SRCFILES = memdump.c
DEFS=-Dlint
UNINITOK = memdump.c
UNUSEDOK = memdump.c
BADFORMATOK = memdump.c

export DOINSTALL=yes

EXTRAINC = -I$(TOP)/lib/libc
EXTRAOBJ = $(TOP)/lib/libutil/obj/libutil.a
include $(TOP)/GNUmakefile.global
