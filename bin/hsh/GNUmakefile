TOP = ../..
PROG = hsh
SRCFILES = hsh.c 
#STATIC=1
# EXTRAOBJ = ../../lib/libc/obj/libc.a ../../lib/ffs/obj/libffs.a

# LIBS = ../../lib/libc/obj/libc.a
EXTRAINC= -I$(TOP)/lib/libexos -I$(TOP)/lib/libc/os

export DOINSTALL=yes

include $(TOP)/GNUmakefile.global

WFLAGS =  -Wall -Wno-comments -Wno-parentheses

#ifndef STATIC

#hsh: hsh.o bounce.o ../shlib/$(STUB)
#	gcc -o hsh -e ___start -nostdlib -N -T $(ENTRY) -g -O6 bounce.o ../../bin/shlib/$(STUB) hsh.o 
#	cp hsh ~/exobin/bin/newer-hsh

#endif
