
TOP = ../../..
PROG = unlink
MAINSRC = unlink.c
# I may include other xok files while testing or fixing the syscall
SRCFILES = $(MAINSRC)

EXTRAINC = -I$(TOP)/lib/libc  -I$(TOP)/lib/libc/fd/nfs

export DOINSTALL=yes
export INSTALLPREFIX=test

include $(TOP)/GNUmakefile.global

EXTRACLN += unix

unix: $(MAINSRC)
	gcc -o unix $(MAINSRC)
