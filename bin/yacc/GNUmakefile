#	$OpenBSD: Makefile,v 1.2 1996/04/21 23:45:02 deraadt Exp $

TOP =     ../../
PROG=	  yacc
SRCFILES= closure.c error.c lalr.c lr0.c main.c mkpar.c output.c reader.c \
	  skeleton.c symtab.c verbose.c warshall.c
WARNOK += $(SRCFILES)
EXTRAINC += -I$(TOP)/lib/libc
export INSTALLPREFIX=usr/bin
export DOINSTALL=yes
include $(TOP)/GNUmakefile.global