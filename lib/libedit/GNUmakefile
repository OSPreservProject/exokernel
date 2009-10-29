
TOP = ../..
OBJDIR = obj
STATIC=1
EXTRACLN = common.h emacs.h fcns.h help.h vi.h \
	 help.c fcns.c editline.c

EXTRAINC = -I.. -I../../libc  -I.
LIB = libedit.a
DEFS+=#-DDEBUG_TTY -DDEBUG_KEY -DDEBUG_READ -DDEBUG -DDEBUG_REFRESH
DEFS+=#-DDEBUG_PASTE
DOINSTALL=yes
UNINITOK =
UNUSEDOK =

OSRCS=	chared.c common.c el.c emacs.c hist.c key.c map.c parse.c \
	prompt.c read.c refresh.c search.c sig.c term.c tty.c vi.c \
	help.c fcns.c
WARNOK= el.c

# For speed and debugging
SRCFILES=   ${OSRCS} tokenizer.c history.c
# For protection
#SRCFILES=	editline.c tokenizer.c history.c

UNUSEDOK = $(SRCFILES) # stupid sccsid

AHDR=vi.h emacs.h common.h 
ASRC=$(CURDIR)/vi.c $(CURDIR)/emacs.c $(CURDIR)/common.c
include $(TOP)/GNUmakefile.global

$(SRCFILES:.c=.o): vi.h emacs.h common.h fcns.h help.h help.c


vi.h: vi.c makelist
	sh $(CURDIR)/makelist -h $(CURDIR)/vi.c > $@

emacs.h: emacs.c makelist
	sh $(CURDIR)/makelist -h $(CURDIR)/emacs.c > $@

common.h: common.c makelist
	sh $(CURDIR)/makelist -h $(CURDIR)/common.c > $@

fcns.h: ${AHDR} makelist
	sh $(CURDIR)/makelist -fh ${AHDR} > $@

fcns.c: ${AHDR} fcns.h makelist
	sh $(CURDIR)/makelist -fc ${AHDR} > $@

help.c: ${ASRC} makelist 
	sh $(CURDIR)/makelist -bc ${ASRC} > $@

help.h: ${ASRC} makelist
	sh $(CURDIR)/makelist -bh ${ASRC} > $@

editline.c: ${OSRCS}
	sh $(CURDIR)/makelist -e ${OSRCS} > $@

#test:	libedit.a test.o 
#	${CC} ${CFLAGS} ${.ALLSRC} -o $@ libedit.a ${LDADD} -ltermcap

#includes:
#	-cd $(CURDIR); cmp -s histedit.h ${DESTDIR}/usr/include/histedit.h > \
#	    /dev/null 2>&1 || \
#	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 histedit.h \
#	    ${DESTDIR}/usr/include
