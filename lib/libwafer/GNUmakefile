
TOP = ../..
DOINSTALL = yes
EXTRAINSTALL = crt0.o
EXTRAINSTALLPREFIX = usr/lib
EXTRATRG = crt0.o

include $(TOP)/GNUmakefile.global

crt0.o: xrt0.o process.o entry.o fault.o bzero.o kprintf.o
	$(LD) -r -o $@ $^
