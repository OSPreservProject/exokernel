STATIC=1
TOP = ../..
PROG = emulate
SRCFILES = handler.c emulate.c interim_syscalls.c emubrk.c emustat.c emu.S \
	debug.c ioctl_table.c
LDADDR = 1900020

export DOINSTALL=yes

EXTRAINC = -I../../lib/libexos
include $(TOP)/GNUmakefile.global
