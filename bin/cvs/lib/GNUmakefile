TOP = ../../..
OBJDIR=obj
STATIC=1
LIB=libcvs.a
SRCFILES =      vasprintf.c regex.c \
    argmatch.c \
    getline.c \
    getopt.c \
    getopt1.c \
    md5.c \
    savecwd.c \
    sighandle.c \
    strippath.c \
    stripslash.c \
    xgetwd.c \
    yesno.c \
    getdate.c

export INSTALLPREFIX=usr/lib

DEFS=-Dlint -DHAVE_CONFIG_H
UNINITOK = $(SRCFILES)
UNUSEDOK = $(SRCFILES)

EXTRAINC = -I$(TOP)/lib/libc -I./. -I../.. -I../../lib -I../../src
include $(TOP)/GNUmakefile.global
