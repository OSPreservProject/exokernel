
TOP = ../../../..
PROG = webdemo
SRCFILES = webdemo.c ../dpf-ir.c ../fast_alloc.c test2.c stat2.c stat.c
#SRCFILES += web_tcp.c
DEFS = -DCLIENT_TCP -DPRINTF_LEVEL=-1
#LIBS = $(TOP)/lib/alfs/libalfssep.a $(TOP)/lib/libc/obj/libc.a
LIBS = $(TOP)/lib/xio/obj/libxio.a $(TOP)/lib/libc/obj/libc.a

export DOINSTALL=yes
export INSTALLPREFIX=bin

EXTRAINC = -I$(TOP)/lib/libc -I$(TOP)/lib -I..
include $(TOP)/GNUmakefile.global
WFLAGS = -Wall
