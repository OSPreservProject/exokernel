# libpwafer does not work w/o GNU LD... OpenBSD ld messes with 
# absolute address defs such as __envs

TOP = ../../..
CRT0 = $(TOP)/lib/libpwafer/obj/crt0.o
CL = $(CC) -static -N -Ttext 800020 -nostdlib 
LIBS = $(TOP)/lib/libpwafer/obj/libpwafer.a

OBJDIR = obj
SRCFILES += simple.c

EXTRATRG = simple
EXTRACLN = $(EXTRATRG)
EXTRAINSTALL = $(EXTRATRG)
EXTRAINSTALLPERMS = 0755

DOINSTALL = yes
EXTRAINSTALLPREFIX = /usr/local/pwafer/tests


include $(TOP)/GNUmakefile.global

simple: simple.o $(CRT0) $(LIBS)
	$(CL) -o simple $(CRT0) simple.o $(LIBS)
	cp simple simple.aout
	$(TOP)/tools/setmagic/setmagic simple 700

