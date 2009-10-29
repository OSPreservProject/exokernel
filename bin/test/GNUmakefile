TOP = ../..
# this should be renamed to test.
PROG = test
SRCFILES = test.c
DEFS=-Dlint
UNINITOK = test.c
UNUSEDOK = test.c

export DOINSTALL=yes

EXTRAINC = -I../../lib/libc
EXTRACLN += [
include $(TOP)/GNUmakefile.global

install::
	$(LN) -f $(EXODEST)/$(INSTALLPREFIX)/$(PROG) $(EXODEST)/$(INSTALLPREFIX)/[

