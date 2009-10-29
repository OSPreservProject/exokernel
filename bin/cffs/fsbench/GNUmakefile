
TOP = ../../..
PROG = fsbench
SRCFILES = fsbench.c locality.c utility.c metadataSeek.c metadataFFS.c

VPATH+=$(TOP)/lib/libc/fd/cffs

LIBS = $(TOP)/lib/libm/obj/libm.a $(TOP)/lib/libc/obj/libc.a

export DOINSTALL=yes
export INSTALLPREFIX=usr/cffs

EXTRAINC = -I$(TOP)/lib/libexos -I$(TOP)/lib
include $(TOP)/GNUmakefile.global
