
TOP = ../../../..
PROG = webfs-simple-test
SRCFILES = webfs-simple-test.c ../webfscalls.c ../webfs_rdwr.c ../webfs_header.c

EXTRAOBJ = $(TOP)/lib/alfs/libalfssep.a
EXTRAINC = -I$(TOP)/lib/libc -I$(TOP)/lib -I..

export DOINSTALL=yes
export INSTALLPREFIX=bin

include $(TOP)/GNUmakefile.global
