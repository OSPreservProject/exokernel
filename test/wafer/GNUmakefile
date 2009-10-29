# libwafer does not work w/o GNU LD... OpenBSD ld messes with 
# absolute address defs such as __envs

TOP = ../..
CRT0 = $(TOP)/lib/libwafer/crt0.o
CL = $(CC) -static -N -Ttext 800020 -nostdlib

EXTRATRG = wafer-test
EXTRACLN = $(EXTRATRG)
EXTRAINSTALL = $(EXTRATRG)
EXTRAINSTALLPERMS = 0755

DOINSTALL = yes
EXTRAINSTALLPREFIX = /test

include $(TOP)/GNUmakefile.global

wafer-test: main.o $(CRT0)
	$(CL) -o wafer-test $(CRT0) main.o
	$(TOP)/tools/setmagic/setmagic wafer-test 700
