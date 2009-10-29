TOP = ../..
PROG = login
SRCFILES = login.c failedlogin.c
DEFS=-Dlint

export DOINSTALL=yes
export INSTALLPREFIX=usr/bin

EXTRAINC = -I../../lib/libc
EXTRAOBJ = ../../lib/libutil/obj/libutil.a
include $(TOP)/GNUmakefile.global
