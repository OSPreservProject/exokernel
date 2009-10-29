#	$OpenBSD: Makefile,v 1.4 1997/04/27 20:56:46 millert Exp $
#
# By default, flex will be configured to generate 8-bit scanners only if the
# -8 flag is given.  If you want it to always generate 8-bit scanners, add
# "-DDEFAULT_CSIZE=256" to CFLAGS.  Note that doing so will double the size
# of all uncompressed scanners.
# 
# If on your system you have trouble building flex due to 8-bit character
# problems, remove the -8 from FLEX_FLAGS and the "#define FLEX_8_BIT_CHARS"
# from the beginning of flexdef.h.
#
# To bootstrap lex, cp initscan.c to scan.c and run make.

TOP =   ../..
PROG=	lex
SRCFILES = ccl.c dfa.c ecs.c gen.c main.c misc.c nfa.c parse.c sym.c tblcmp.c \
	yylex.c scan.o skel.c

LIBS += libl.a

parse.h parse.c: parse.y
	yacc -d parse.y
	mv y.tab.c parse.c
	mv y.tab.h parse.h

skel.c:	flex.skl mkskel.sh
	sh mkskel.sh flex.skl > skel.c

#scan.c: scan.l
#	flex -t -p ${.CURDIR}/scan.l > scan.c


export INSTALLPREFIX=usr/bin
export DOINSTALL=yes
include $(TOP)/GNUmakefile.global