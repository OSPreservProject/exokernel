TOP = ../..

PROG = inetd
SRCFILES = inetd.c
UNUSEDOK = inetd.c

VPATH += ../loadkern
SRCFILES += loadkern.c exec.c

# internal loadkern and reboot commands
DEFS += -DILOADKERN -DIREBOOT

export DOINSTALL=yes
export INSTALLPREFIX=usr/sbin

include $(TOP)/GNUmakefile.global
