
TOP = ../../..

DOINSTALL=yes
VOS = $(TOP)/lib/vos
OBJDIR = $(VOS)/obj
DEFS=-DFLOATING_POINT=1 -DNOTEXOS -D__LIBVOS__
STATIC = 1

LIB = libvos.a 
EXTRATRG = crt0.o crt0_initp.o crt0_cpp.o
EXTRAINSTALL = crt0.o crt0_initp.o crt0_cpp.o
EXTRAINSTALLPREFIX = vos


VPATH += $(VOS)/os
SRCFILES += compat.c 

VPATH += $(VOS)/os
SRCFILES += fault.c proc.c fork.c entry.S ipc.S cipc.c vm.c kprintf.c perf.S \
	sbuf.c malloc.c wk.c fd.c ipcport.c uinfo.c signals.c \
	gettimeofday.c uidt.c fpu.c

VPATH += $(VOS)/os/net
SRCFILES += iptable.c arptable.c ae_ether.c arp.c cksum.c

VPATH += $(VOS)/os/dpf
SRCFILES += dpf.c dpf-ir.c


include $(TOP)/lib/vos/modules/GNUmakefile.inc

include $(TOP)/GNUmakefile.global


xrt0.o:	$(LIB)

xrt0_cpp.o: xrt0.c $(LIB)
	$(CC_COMPILE_RULE) -o xrt0_cpp.o -D__CPP_PROG__

xrt0_initp.o: xrt0.c $(LIB)
	$(CC_COMPILE_RULE) -o xrt0_initp.o -D__INIT_PROG__

crt0_initp.o: xrt0_initp.o
	$(LD) -r -o $@ $^

crt0_cpp.o: xrt0_cpp.o
	$(LD) -r -o $@ $^

crt0.o: xrt0.o
	$(LD) -r -o $@ $^


