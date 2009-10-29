
TOP = ../..
PROG = ftp
SRCFILES = cmds.c cmdtab.c complete.c domacro.c fetch.c ftp.c main.c \
	ruserpass.c stringlist.c util.c
UNUSEDOK = cmdtab.c complete.c domacro.c fetch.c ftp.c main.c ruserpass.c \
	util.c
WARNOK = cmds.c

EXTRAOBJ += $(TOP)/lib/libedit/obj/libedit.a
EXTRAOBJ += $(TOP)/lib/libcurses/obj/libcurses.a

export DOINSTALL=yes
export INSTALLPREFIX=usr/bin

include $(TOP)/GNUmakefile.global
