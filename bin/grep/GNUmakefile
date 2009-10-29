TOP = ../..
PROG = grep
SRCFILES = dfa.c grep.c kwset.c obstack.c regex.c search.c # getopt.c 
DEFS=-Dlint -DGREP -DHAVE_STRING_H=1 -DHAVE_SYS_PARAM_H=1 -DHAVE_UNISTD_H=1 \
        -DHAVE_GETPAGESIZE=1 -DHAVE_MEMCHR=1 -DHAVE_STRERROR=1 \
        -DSTDC_HEADERS
UNINITOK = $(SRCFILES)
UNUSEDOK = $(SRCFILES)
WARNOK = dfa.c

export DOINSTALL=yes
export INSTALLPREFIX=usr/bin

EXTRAINC += -I$(TOP)/lib/libc
EXTRACLN += egrep fgrep
include $(TOP)/GNUmakefile.global

check:	all
	sh ${CURDIR}/tests/check.sh ${CURDIR}/tests

install::
	$(LN) -f $(EXODEST)/$(INSTALLPREFIX)/$(PROG) $(EXODEST)/$(INSTALLPREFIX)/egrep
	$(LN) -f $(EXODEST)/$(INSTALLPREFIX)/$(PROG) $(EXODEST)/$(INSTALLPREFIX)/fgrep

