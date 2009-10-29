TOP = ../..
PROG = videod
SRCFILES = videod.c xio_helper.c 

export DOINSTALL=yes
export INSTALLPREFIX=usr/bin
export EXTRAINSTALLPREFIX = usr/lib/mpeg
export EXTRAINSTALL = wallace.mpg

EXTRAINC = -I../../lib/libc 
include $(TOP)/GNUmakefile.global

WFLAGS += -Wno-error
 

