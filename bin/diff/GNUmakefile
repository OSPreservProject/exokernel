TOP = ../..
PROG = diff
SRCFILES=diff.c analyze.c cmpbuf.c io.c context.c ed.c normal.c util.c dir.c \
	regex.c side.c ifdef.c version.c getopt.c getopt1.c
DEFS=-Dlint
DEFS+=-DHAVE_CONFIG_H -I${.CURDIR} -DPR_PROGRAM=\"/usr/bin/pr\"
UNINITOK = $(SRCFILES)
UNUSEDOK = $(SRCFILES)

export DOINSTALL=yes
export INSTALLPREFIX=usr/bin

EXTRAINC = -I../../lib/libc -I.
include $(TOP)/GNUmakefile.global
