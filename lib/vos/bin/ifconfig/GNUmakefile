
# libvos does not work w/o GNU LD... OpenBSD ld messes with 
# absolute address defs such as __envs

TOP = ../../../..
DEFS=-DNOTEXOS

CRT0 = $(TOP)/lib/vos/obj/crt0.o
LIBM = $(TOP)/lib/libm/obj/libm.a 
LIBVOS = $(TOP)/lib/vos/obj/libvos.a 
LIBC = $(TOP)/lib/vos/voslibc/obj/libc.a 
LIBS = $(LIBVOS) $(LIBC) $(LIBM) $(LIBVOS)

CL = $(CC) -static -N -Ttext 800020 -nostdlib 
CP = cp

SRCFILES += ifconfig.c

EXTRATRG = vifconfig
EXTRACLN = $(EXTRATRG)
EXTRAINSTALL = $(EXTRATRG)
EXTRAINSTALLPERMS = 0755

DOINSTALL = yes
EXTRAINSTALLPREFIX = usr/local/bin

include $(TOP)/GNUmakefile.global


vifconfig: ifconfig.o $(CRT0) $(LIBS)
	$(CL) -o vifconfig $(CRT0) ifconfig.o $(LIBS)
	cp vifconfig vifconfig.aout
	$(TOP)/tools/setmagic/setmagic vifconfig 700

