
TOP = ../..
PROG = bootp.b.gdb
SRCFILES = bootptest.c bootstrap.c print-bootp.c getether.c
EXTRATRG = bootp.b.s bootp.b.gdb
EXTRAOBJ = ../../lib/libc/obj/libc.a 

DEFS+= -Dlint
export DOINSTALL=yes


VPATH += $(TOP)/bin/arpd
SRCFILES += arpd.c

STATIC=1

EXTRAINC = -I../../lib/libexos
include $(TOP)/GNUmakefile.global
