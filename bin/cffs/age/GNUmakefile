
TOP = ../../..
PROG = age
SRCFILES = age.c
LIBS = $(TOP)/lib/libc/obj/libc.a

export DOINSTALL=yes
export INSTALLPREFIX=usr/cffs

EXTRAINC = -I$(TOP)/lib/libc -I$(TOP)/lib
include $(TOP)/GNUmakefile.global

unix:
	gcc -O2 -Wall age.c -o unix
