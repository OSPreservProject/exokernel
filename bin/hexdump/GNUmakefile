TOP = ../..
PROG = hexdump
SRCFILES = conv.c display.c hexdump.c hexsyntax.c odsyntax.c parse.c
WARNOK = conv.c display.c hexdump.c hexsyntax.c odsyntax.c parse.c

export DOINSTALL=yes
export INSTALLPREFIX=usr/bin

EXTRAINC = -I../../lib/libc -L.
include $(TOP)/GNUmakefile.global

install::
	$(LN) -f $(EXODEST)/$(INSTALLPREFIX)/$(PROG) $(EXODEST)/$(INSTALLPREFIX)/od
