TOP = ../..
STATIC=1
PROG = hardcoded.b.gdb
SRCFILES = hardcoded.c 
EXTRATRG = hardcoded.b.s hardcoded.b.gdb
EXTRAOBJ = ../../lib/libc/obj/libc.a 

LIBS = ../../lib/libc/obj/libc.a

VPATH += $(TOP)/bin/arpd
SRCFILES += arpd.c

EXTRAINC = -I$(TOP)/lib/libexos 
EXTRAINC += -DRCLOCAL=\""/etc/rc.local\""

include $(TOP)/GNUmakefile.global


install:: all
