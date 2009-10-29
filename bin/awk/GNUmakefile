
TOP = ../..
PROG = awk
SRCFILES = ytab.c lex.c b.c main.c parse.c proctab.c tran.c lib.c run.c
UNUSEDOK = ytab.c
LIBS += $(TOP)/lib/libm/obj/libm.a

export DOINSTALL=yes
export INSTALLPREFIX=usr/bin

PREORDER = yes
EXTRACLN = proctab.c maketab ytab.c ytab.h

include $(TOP)/GNUmakefile.global

install::
	$(INSTALL) -d $(EXODEST)/bin
	$(INSTALL) -c -s -m 0755 awk $(EXODEST)/bin/nawk

ytab.c ytab.h: awkgram.y
	$(YACC) -d $(CURDIR)/awkgram.y
	mv y.tab.c ytab.c
	mv y.tab.h ytab.h

proctab.c: maketab
	./maketab >proctab.c

MTCFLAGS+=-I. -I$(CURDIR)
maketab: ytab.h maketab.c
	$(NCC) $(MTCFLAGS) $(CURDIR)/maketab.c -o maketab
