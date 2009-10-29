TOP = ../..
STATIC=1
PROG = kbdinit.b.gdb
SRCFILES = kbdinit.c 
EXTRATRG = kbdinit.b.s kbdinit.b.gdb
EXTRAOBJ = ../../lib/libc/obj/libc.a 

LIBS = ../../lib/libc/obj/libc.a

VPATH += $(TOP)/bin/arpd
SRCFILES += arpd.c


EXTRAINC = -I$(TOP)/lib/libc -I../rconsoled
# maybe dave can make this nicer
EXTRAINC += -DSTARTUPSHELL="\"/$(USER)/bin/hsh\"" -DRCLOCAL=\""/etc/rc.local\""

include $(TOP)/GNUmakefile.global


install:: all
