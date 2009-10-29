
TOP = ../..
PROG = telnet
SRCFILES = authenc.c commands.c main.c network.c ring.c sys_bsd.c \
	telnet.c terminal.c tn3270.c utilities.c

DEFS += -DTERMCAP -DKLUDGELINEMODE -DUSE_TERMIO -DSKEY -Dunix
DEFS += -DENV_HACK -Dlint

export DOINSTALL=yes
export INSTALLPREFIX=usr/bin

EXTRAINC = -I../../lib/libc

EXTRAOBJ = $(TOP)/lib/libterm/obj/libtermcap.a \
	   $(TOP)/lib/libtelnet/obj/libtelnet.a
           

include $(TOP)/GNUmakefile.global
WFLAGS = 