TOP=		../../..

LIB=		libreadline.a
OBJDIR= 	obj
STATIC= 	1

EXTRAINC=	-I../../include/ \
		-I../$(TOP)/lib/libc \
		-I../ \
		-I../$(TOP)/include

DEFS+=		-DVOID_SIGHANDLER -DVI_MODE -DHAVE_UNISTD_H

WARNOK=		funmap.c history.c keymaps.c readline.c vi_mode.c tilde.c

SRCFILES=	funmap.c history.c keymaps.c readline.c vi_mode.c tilde.c

include $(TOP)/GNUmakefile.global



