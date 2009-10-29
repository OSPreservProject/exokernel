TOP = ../..
CC_VER = -V 2.8.1
OBJDIR = obj
LIB = libudf.a
STATIC=1
DEFS = -DKERNEL #-DNDEBUG
ifdef XOK_SMP
DEFS += -D__SMP__
endif

EXTRAINC = -I$(TOP)/sys/ubb -I$(TOP)/sys/ubb/lib -I$(TOP)/sys/ubb/kernel 
INC = -nostdinc -I$(TOP)/sys -I. $(EXTRAINC)

SRCFILES += bootstrap.c check.c dependency.c init.c interp.c iov.c mv.c \
	registry.c root-catalogue.c template.c type.c ubb-dis.c udf.c xn.c \
	xn-sys.c rules.c

VPATH = lib
SRCFILES += set.c bit.c arena.c table.c ubb-lib.c

VPATH += kernel
SRCFILES += real-kernel.c bc.c

DOINSTALL=no

include $(TOP)/GNUmakefile.global


