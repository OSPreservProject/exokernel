TOP = ../..
PROG = passwd
SRCFILES = passwd.c local_passwd.c

DEFS=-Dlint
UNINITOK = passwd.c
UNUSEDOK = passwd.c

export DOINSTALL=yes

EXTRAINC = -I../../lib/libc
EXTRAOBJ = ../../lib/libutil/obj/libutil.a
include $(TOP)/GNUmakefile.global

WFLAGS = -Wall -Wno-comments -Wno-parentheses

