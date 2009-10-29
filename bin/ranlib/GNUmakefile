
TOP = ../..
PROG = ranlib
VPATH += ../ar
SRCFILES= build.c misc.c ranlib.c touch.c archive.c
UNINITOK = build.c
WARNOK = touch.c archive.c

DEFS = -Dlint

export DOINSTALL=yes
export INSTALLPREFIX=usr/bin

EXTRAINC = -I../../lib/libc -I. -I../ar
include $(TOP)/GNUmakefile.global
