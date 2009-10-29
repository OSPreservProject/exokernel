
TOP = ../..
PROG = ftpd
SRCFILES = ftpd.c ftpcmd.c logutmp.c logwtmp.c popen.c
DEFS += -DHASSETPROCTITLE -Dunix -Dlint
WARNOK += ftpd.c popen.c
EXTRACLN = ftpcmd.c y.tab.h

export DOINSTALL=yes
export INSTALLPREFIX=usr/libexec

include $(TOP)/GNUmakefile.global

ftpcmd.c: ftpcmd.y
	$(YACC) -d ftpcmd.y
	mv y.tab.c ftpcmd.c

