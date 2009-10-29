
# libvos does not work w/o GNU LD... OpenBSD ld messes with 
# absolute address defs such as __envs

TOP = ../../../..
DEFS=-DNOTEXOS

CRT0 = $(TOP)/lib/vos/obj/crt0.o
LIBVOS = $(TOP)/lib/vos/obj/libvos.a 
LIBC = $(TOP)/lib/vos/voslibc/obj/libc.a 
LIBS = $(LIBVOS) $(LIBC) $(LIBVOS) $(TOP)/lib/libm/obj/libm.a 

CL = $(CC) -static -N -Ttext 800020 -nostdlib 
CP = cp

SRCFILES += ps.c kill.c

EXTRATRG = vps vkill
EXTRACLN = $(EXTRATRG)
EXTRAINSTALL = $(EXTRATRG)
EXTRAINSTALLPERMS = 0755

DOINSTALL = yes
EXTRAINSTALLPREFIX = usr/local/bin

include $(TOP)/GNUmakefile.global


vps: ps.o $(CRT0) $(LIBS)
	$(CL) -o vps $(CRT0) ps.o $(LIBS)
	cp vps vps.aout
	$(TOP)/tools/setmagic/setmagic vps 700

vkill: kill.o $(CRT0) $(LIBS)
	$(CL) -o vkill $(CRT0) kill.o $(LIBS)
	$(TOP)/tools/setmagic/setmagic vkill 700

