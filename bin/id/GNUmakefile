
TOP = ../..
PROG = id
SRCFILES = id.c
UNUSEDOK = id.c

export DOINSTALL=yes
export INSTALLPREFIX=usr/bin

EXTRAINC = -I../../lib/libc -L.
include $(TOP)/GNUmakefile.global

install::
	cp groups.sh $(EXODEST)/usr/bin/groups
	cp whoami.sh $(EXODEST)/usr/bin/whoami
