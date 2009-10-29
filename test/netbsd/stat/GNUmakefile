
TOP = ../../..
PROG = stat
MAINSRC = stat.c
# I may include other xok files while testing or fixing the syscall
SRCFILES = $(MAINSRC) 

EXTRAINC = -I$(TOP)/lib/libc -I$(TOP)/lib/libc/fd/npty -I$(TOP)/lib/libc/fd
VPATH += $(TOP)/lib/libc/fd/npty $(TOP)/lib/libc/fd

export DOINSTALL=yes
export INSTALLPREFIX=test

include $(TOP)/GNUmakefile.global

EXTRACLN += unix

unix: $(MAINSRC)
	gcc -ggdb -o $@ $(MAINSRC)
