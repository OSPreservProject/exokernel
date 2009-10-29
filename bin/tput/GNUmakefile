
TOP = ../..
PROG = tput
SRCFILES = tput.c
WARNOK = tput.c

EXTRAOBJ=  $(TOP)/lib/libterm/obj/libtermcap.a

export DOINSTALL=yes
export INSTALLPREFIX=usr/bin

EXTRAINC = -I../../lib/libc -L.
include $(TOP)/GNUmakefile.global

EXTRACLN += tput clear

install::
	$(LN) -f $(EXODEST)/$(INSTALLPREFIX)/$(PROG) $(EXODEST)/$(INSTALLPREFIX)/clear
