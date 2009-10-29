
# libvos does not work w/o GNU LD... OpenBSD ld messes with 
# absolute address defs such as __envs

TOP = ../../../..
DEFS=-DNOTEXOS

CRT0 = $(TOP)/lib/vos/obj/crt0.o
LIBVOS = $(TOP)/lib/vos/obj/libvos.a 
LIBC = $(TOP)/lib/vos/voslibc/obj/libc.a 
LIBS = $(LIBVOS) $(LIBC) $(LIBVOS)

CL = $(CC) -static -N -Ttext 800020 -nostdlib 
CP = cp

SRCFILES += icmpd.c
OBJFILES += icmpd.c

EXTRATRG = vicmpd
EXTRACLN = $(EXTRATRG) $(EXTRATRG).aout
EXTRAINSTALL = $(EXTRATRG)
EXTRAINSTALLPERMS = 0755

DOINSTALL = yes
EXTRAINSTALLPREFIX = usr/local/bin

include $(TOP)/GNUmakefile.global


vicmpd: $(OBJFILES) $(CRT0) $(LIBS)
	$(CL) -o vicmpd $(CRT0) $(OBJFILES) $(LIBS)
	cp vicmpd vicmpd.aout
	$(TOP)/tools/setmagic/setmagic vicmpd 700

