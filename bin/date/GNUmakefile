
TOP = ../..
PROG = date
SRCFILES = date.c netdate.c

export DOINSTALL=yes

EXTRAINC = -I../../lib/libc
EXTRAOBJ = $(TOP)/lib/libutil/obj/libutil.a
DEFS+= -Dlint
include $(TOP)/GNUmakefile.global
