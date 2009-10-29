
TOP = ../..
OBJDIR = obj
STATIC=1
EXTRAINC = -I.. -I../../libc -I.
LIB = libutil.a
DEFS =  -Dlint -DLIBC_SCCS
EXTRACLN = 
UNUSEDOK =

SRCFILES = getmaxpartitions.c getrawpartition.c login.c login_tty.c logout.c \
        logwtmp.c opendev.c passwd.c pty.c readlabel.c scsi.c login_fbtab.c \
	uucplock.c
WARNOK = passwd.c readlabel.c login.c login_fbtab.c
UNINITOK = scsi.c

export DOINSTALL=yes
include $(TOP)/GNUmakefile.global
