
TOP = ../../../..
PROG = webswamp
SRCFILES = webswamp.c
DEFS = -DCLIENT_TCP -DPRINTF_LEVEL=-1

#LIBS = $(TOP)/lib/xio/obj/libxio.a $(TOP)/lib/libc/obj/libcx.a

export DOINSTALL=yes
export INSTALLPREFIX=bin

EXTRAINC = -I$(TOP)/lib/libc -I$(TOP)/lib -I..
include $(TOP)/GNUmakefile.global
