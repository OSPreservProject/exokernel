
TOP = ../../..
PROG = webfs-dumbdaemon
SRCFILES = webfs-dumbdaemon.c webfscalls.c webfs_rdwr.c webfs_header.c
SUBDIRS = webfs-init \
          webfs-readfile \
          webfs-scansep \
          webfs-simple-test \

EXTRAINC = -I$(TOP)/lib/libc -I$(TOP)/lib
EXTRAOBJ = $(TOP)/lib/alfs/libalfssep.a

export DOINSTALL=yes
export INSTALLPREFIX=bin

PREORDER = yes
include $(TOP)/GNUmakefile.global
