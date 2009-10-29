TOP = ../../..
PROG = gethostbyaddr
SRCFILES = main.c

export DOINSTALL=yes
export INSTALLPREFIX=

include $(TOP)/GNUmakefile.global

unix: $(MAINSRC)
	cc -ggdb -pipe -o $@ $(MAINSRC)
