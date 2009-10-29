
TOP = ../..
PROG = md5
SRCFILES = md5.c

export DOINSTALL=yes
export INSTALLPREFIX=bin

include $(TOP)/GNUmakefile.global

install::
	$(LN) -f $(PROG)  $(EXODEST)/$(INSTALLPREFIX)/sha1
	$(LN) -f $(PROG)  $(EXODEST)/$(INSTALLPREFIX)/rmd160
