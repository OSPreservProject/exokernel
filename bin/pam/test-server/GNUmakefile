TOP = ../../..
PROG = test-server
SRCFILES = test-server.c test-server-methods.c

export DOINSTALL=yes
export INSTALLPREFIX=bin

EXTRAINC = -I$(TOP)/lib/libexos 
include $(TOP)/GNUmakefile.global

WFLAGS = -Wno-unused -Wall -Wno-comments -Wno-parentheses
