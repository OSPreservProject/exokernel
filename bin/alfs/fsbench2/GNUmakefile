
TOP = ../../..
PROG = fsbench2
SRCFILES = fsbench2.c locality.c utility.c metadataSeek.c metadataFFS.c
LIBS = $(TOP)/lib/alfs/libalfsemb.a $(TOP)/lib/libm/obj/libm.a $(TOP)/lib/libc/obj/libc.a

export DOINSTALL=yes

EXTRAINC = -I$(TOP)/lib/libc -I$(TOP)/lib
include $(TOP)/GNUmakefile.global
