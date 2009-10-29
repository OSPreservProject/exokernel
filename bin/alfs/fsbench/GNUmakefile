
TOP = ../../..
PROG = fsbench
SRCFILES = fsbench.c locality.c utility.c metadataSeek.c metadataFFS.c
LIBS = $(TOP)/lib/alfs/libalfssep.a $(TOP)/lib/libm/obj/libm.a $(TOP)/lib/libc/obj/libc.a

export DOINSTALL=yes

EXTRAINC = -I$(TOP)/lib/libc -I$(TOP)/lib
include $(TOP)/GNUmakefile.global
