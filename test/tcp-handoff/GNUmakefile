
TOP = ../..
PROG = xdate
MAINSRC = xdate.c

# I may include other xok files while testing or fixing the syscall
VPATH += $(TOP)/lib/libc/net/tcp
SRCFILES = $(MAINSRC) tcp_handoff.c 


EXTRAINC = -I$(TOP)/lib/libc  -I$(TOP)/lib/libc/net/tcp
include $(TOP)/GNUmakefile.global

EXTRACLN += unix

unix: $(MAINSRC)
	gcc -o unix $(MAINSRC)

