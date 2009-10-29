TOP = ../..
PROG = which
SRCFILES = which.c
UNUSEDOK = which.c

export DOINSTALL=yes
export INSTALLPREFIX=/usr/bin

include $(TOP)/GNUmakefile.global

install::
	$(LN) -f $(PROG)  $(EXODEST)/$(INSTALLPREFIX)/whereis
