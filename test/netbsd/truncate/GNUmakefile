
TOP = ../../..
PROG = truncate
SRCFILES = truncate.c

EXTRAINC = -I$(TOP)/lib/libc

export DOINSTALL=yes
export INSTALLPREFIX=test

include $(TOP)/GNUmakefile.global

EXTRACLN+= unix
unix: $(SRCFILES)
	gcc -o unix $(SRCFILES)
