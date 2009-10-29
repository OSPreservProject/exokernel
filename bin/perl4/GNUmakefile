
TOP = ../..
PROG = perl4
SRCFILES = array.c cmd.c cons.c consarg.c doarg.c doio.c dolist.c dump.c \
	eval.c form.c perl.c regcomp.c regexec.c stab.c str.c toke.c util.c \
	hash.c usersub.c perly.c
EXTRAINC = -I$(TOP)/include -I$(TOP)/lib/libc -I$(TOP)/sys
EXTRAOBJ = $(TOP)/lib/libm/obj/libm.a 

export DOINSTALL=yes
export INSTALLPREFIX=usr/local/bin

include $(TOP)/GNUmakefile.global

%.o : %.c
	$(CC) -c -MD -pipe -g -O6 -nostdinc $(EXTRAINC) $<

install::
	$(LN) -f $(EXODEST)/$(INSTALLPREFIX)/$(PROG) $(EXODEST)/usr/bin/perl
