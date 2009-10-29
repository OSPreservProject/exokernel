
TOP = ../..

PW = $(TOP)/lib/libpwafer
OBJDIR = $(PW)/obj
DEFS=-DFLOATING_POINT=1 -DNOTEXOS
STATIC = 1

LIB = libpwafer.a 
EXTRATRG = crt0.o 

VPATH += $(PW)
SRCFILES += threads.c

VPATH += $(PW)
SRCFILES += bzero.S entry.S fault.c kprintf.c process.c memset.S

include $(TOP)/GNUmakefile.global


crt0.o: xrt0.o
	$(LD) -r -o $@ $^


