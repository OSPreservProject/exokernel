TOP = ../../..
PROG = rmalfs2
SRCFILES = scanemb.c
DEFS += -DRMALFS

export DOINSTALL=yes

LIBS = $(TOP)/lib/alfs/libalfsemb.a $(TOP)/lib/libc/obj/libc.a
EXTRAINC = -I$(TOP)/lib/libc -I$(TOP)/lib
include $(TOP)/GNUmakefile.global

WFLAGS += -Wno-error

