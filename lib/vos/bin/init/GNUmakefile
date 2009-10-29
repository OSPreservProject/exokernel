
# libvos does not work w/o GNU LD... OpenBSD ld messes with 
# absolute address defs such as __envs

TOP = ../../../..
DEFS=-DNOTEXOS

CRT0 = $(TOP)/lib/vos/obj/crt0_initp.o
LIBVOS = $(TOP)/lib/vos/obj/libvos.a 
LIBC = $(TOP)/lib/vos/voslibc/obj/libc.a 
LIBS = $(LIBVOS) $(LIBC) $(LIBVOS)

CL = $(CC) -static -N -Ttext 800020 -nostdlib 
CP = cp

SRCFILES += init.c arptable.c iptable.c arp.c 
OBJFILES += init.o arptable.o iptable.o arp.o

EXTRATRG = vinit
EXTRACLN = $(EXTRATRG) $(EXTRATRG).aout
EXTRAINSTALL = $(EXTRATRG)
EXTRAINSTALLPERMS = 0755

DOINSTALL = yes
EXTRAINSTALLPREFIX = usr/local/bin

include $(TOP)/GNUmakefile.global


vinit: $(OBJFILES) $(CRT0) $(LIBS)
	$(CL) -o vinit $(CRT0) $(OBJFILES) $(LIBS)
	cp vinit vinit.aout
	$(TOP)/tools/setmagic/setmagic vinit 700

