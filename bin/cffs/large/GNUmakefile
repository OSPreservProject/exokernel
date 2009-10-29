
TOP = ../../..
PROG = large
SRCFILES = large.c
LIBS = $(TOP)/lib/libc/obj/libc.a

export DOINSTALL=yes
export INSTALLPREFIX=usr/cffs

EXTRAINC = -I$(TOP)/lib/libexos -I$(TOP)/lib
include $(TOP)/GNUmakefile.global

unix:
	gcc -O2 -Wall large.c -o unix
