
TOP = ../../..
PROG = small
SRCFILES = small.c

VPATH+=$(TOP)/lib/libc/fd/cffs

LIBS = $(TOP)/lib/libc/obj/libc.a

export DOINSTALL=yes
export INSTALLPREFIX=usr/cffs

EXTRAINC = -I$(TOP)/lib/libexos -I$(TOP)/lib
include $(TOP)/GNUmakefile.global

unix: small.c
	gcc -O2 -Wall small.c -o unix
