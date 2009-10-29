
TOP = ../..
DOINSTALL = yes
EXTRAINSTALL = crt0.o crt0_initp.o
EXTRAINSTALLPREFIX = usr/lib
EXTRAINC = -I$(TOP)/lib/libexos
EXTRATRG = crt0.o crt0_initp.o

VPATH = ../ash

include $(TOP)/GNUmakefile.global

ashpad.o: bounce.o
	$(TOP)/tools/pad/pad bounce.o > ashpad.s
	$(AS) -o ashpad.o ashpad.s

# remove the hash mark from the following line to include ashes
crt0.o: bounce.o # ashpad.o ash.b.o ash.sym.o
	$(LD) -r -o $@ $^

bounce_initp.o: bounce.c
	$(CC_COMPILE_RULE) -o bounce_initp.o -D__INITPROG__

bounce_initp.s: bounce.c
	$(CC_COMPILE_2ASM_RULE) -o bounce_initp.s -D__INITPROG__

crt0_initp.o: bounce_initp.o # ashpad.o ash.b.o ash.sym.o
	$(LD) -r -o $@ $^
