TOP = ../..
PROG = ld
SRCFILES = ld.c symbol.c lib.c shlib.c warnings.c etc.c rrs.c xbits.c md.c
EXTRAINC = -I. -I./i386 -I../../lib/libc
export DOINSTALL=yes
export INSTALLPREFIX=usr/bin
DEFS+= -DDEFAULT_MAGIC=OMAGIC
include $(TOP)/GNUmakefile.global
ifdef LDADDR
DEFS+= -DTEXT_START_ADDRESS=0x$(LDADDR)
endif


WFLAGS = 
