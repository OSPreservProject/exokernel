TOP = ../../..
PROG = rmalfs
SRCFILES = scansep.c
DEFS += -DRMALFS

export DOINSTALL=yes

LIBS = $(TOP)/lib/alfs/libalfssep.a $(TOP)/lib/libc/obj/libc.a
EXTRAINC = -I$(TOP)/lib/libc -I$(TOP)/lib
include $(TOP)/GNUmakefile.global

WFLAGS += -Wno-error

