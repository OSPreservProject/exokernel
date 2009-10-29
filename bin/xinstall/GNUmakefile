
TOP = ../..
PROG = xinstall
SRCFILES = stat_flags.c xinstall.c
VPATH += ../ls
DEFS+= -Dlint

export DOINSTALL=yes

EXTRAINC = -I../../lib/libc
include $(TOP)/GNUmakefile.global
WFLAGS=

install::
	ln -f $(EXODEST)/$(INSTALLPREFIX)/$(PROG)  $(EXODEST)/$(INSTALLPREFIX)/install


