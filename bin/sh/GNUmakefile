
TOP = ../..

PROG=	sh
SRCFILES= alias.c builtins.c cd.c echo.c error.c eval.c exec.c expand.c \
	histedit.c input.c jobs.c mail.c main.c memalloc.c miscbltin.c \
	mystring.c nodes.c options.c parser.c redir.c  show.c syntax.c \
	trap.c output.c var.c arith.c arith_lex.c init.c 
WARNOK += parser.c arith.c mknodes.c mkinit.c mksyntax.c error.c \
	arith_lex.c
UNUSEDOK += input.c
AUXPROGS = mksyntax mknodes mkinit
DPADD+=	${LIBL} ${LIBEDIT} ${LIBTERMCAP}
LDLAGS += -8	# 8-bit lex scanner for arithmetic
DEFS +=-DSHELL -Dlint
EXTRAINC = -I../../lib/libc

LIBS =	$(TOP)/lib/libl/obj/libl.a \
	$(TOP)/lib/libedit/obj/libedit.a \
	$(TOP)/lib/libterm/obj/libtermcap.a \
	$(TOP)/lib/libcurses/obj/libcurses.a

export DOINSTALL=yes

include $(TOP)/GNUmakefile.global

token.def: mktokens
	sh mktokens

builtins.h builtins.c: mkbuiltins builtins.def
	sh mkbuiltins .

init.c: mkinit $(filter-out init.c,$(SRCFILES))
	./mkinit $(filter-out init.c arith.c arith_lex.c, $(SRCFILES))

#mkinit: mkinit.c
#	$(CC) ${CFLAGS} mkinit.c -o $@

nodes.c nodes.h: mknodes nodetypes nodes.c.pat
	./mknodes nodetypes nodes.c.pat

#mknodes: mknodes.c
#	${CC} ${CFLAGS} mknodes.c -o $@

syntax.c syntax.h: mksyntax
	./mksyntax

#mksyntax: mksyntax.c parser.h
#	${CC} ${CFLAGS} mksyntax.c -o $@

w:
	@echo someone is calling make w but I dont understand

