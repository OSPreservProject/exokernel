TOP = ../..
PROG = telnetd
SRCFILES = authenc.c global.c slc.c state.c sys_term.c telnetd.c \
	termstat.c utility.c
UNUSEDOK = authenc.c global.c slc.c state.c sys_term.c telnetd.c \
	termstat.c utility.c

INC=-I. -nostdinc -I$(TOP)/include -I$(TOP)/sys -I. -I$(TOP)/lib/libc -Isys

EXTRAOBJ = $(TOP)/lib/libcurses/obj/libcurses.a \
	$(TOP)/lib/libtelnet/obj/libtelnet.a \
	$(TOP)/lib/libutil/obj/libutil.a

DEFS+=-DEXOPC
DEFS+=-DLINEMODE -DKLUDGELINEMODE -DUSE_TERMIO -DDIAGNOSTICS
DEFS+=-DOLD_ENVIRON -DENV_HACK -I${.CURDIR}
DEFS+=-D_USE_OLD_CURSES_

# These are the sources that have encryption stuff in them.
CRYPT_SRC= authenc.c ext.h state.c telnetd.c termstat.c
CRYPT_SRC+= utility.c Makefile
NOCRYPT_DIR=${.CURDIR}/Nocrypt

export DOINSTALL=yes
export INSTALLPREFIX=usr/libexec

include $(TOP)/GNUmakefile.global

nocrypt:
	@echo "Encryption code already removed."

WFLAGS = -Wall -Wno-comments -Wno-parentheses
