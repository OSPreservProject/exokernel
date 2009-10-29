TOP = ../..
PROG = libexos.so
EXTRAINSTALL = $(PROG)_stub.o
EXTRAINSTALLPREFIX = usr/lib
include $(TOP)/ARCH

$(PROG)_stub.o: $(PROG) $(TOP)/tools/mkspec/mkspec
	$(TOP)/tools/mkspec/mkspec $(PROG) > $(PROG)_stub.s
	$(CC) -c $(PROG)_stub.s

T = -Ttext

$(PROG): main.c $(TOP)/lib/libc/obj/libc.a $(TOP)/lib/csu/crt0.o 
	# this load address has to be in agreement with exos/vm-layout.h
	$(CC) -Wall -o $(PROG)  -nostdinc -DEXOPC -I../../include -I../../sys \
	-I../../lib/libc -static $(T) 10000020 -N -g -O6 -nostdlib \
	-e ___start $(TOP)/lib/csu/crt0.o  \
	$(TOP)/lib/libc/obj/*.o main.c 

install:: $(PROG)_stub.o

EXTRACLN = $(PROG) $(PROG)_stub.s

export DOINSTALL=yes
export INSTALLPREFIX=usr/lib

include $(TOP)/GNUmakefile.global 
