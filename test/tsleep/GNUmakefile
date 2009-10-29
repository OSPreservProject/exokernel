TOP = ../..
PROG = tsleep-test
SRCFILES = tsleep.c
export DOINSTALL=yes

ifeq (1,0)
STATIC=1
EXTRAINC = -I$(TOP)/lib/libexos
VPATH +=  $(TOP)/lib/libexos/os
SRCFILES += synch.c
endif
include $(TOP)/GNUmakefile.global
