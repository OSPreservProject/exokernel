
TOP = ../..
PROG = make
SRCFILES =  arch.c buf.c compat.c cond.c dir.c for.c hash.c job.c main.c \
        make.c parse.c str.c suff.c targ.c var.c util.c
VPATH += lst.lib
SRCFILES += lstAppend.c lstAtEnd.c lstAtFront.c lstClose.c lstConcat.c \
        lstDatum.c lstDeQueue.c lstDestroy.c lstDupl.c lstEnQueue.c \
        lstFind.c lstFindFrom.c lstFirst.c lstForEach.c lstForEachFrom.c \
        lstInit.c lstInsert.c lstIsAtEnd.c lstIsEmpty.c lstLast.c \
        lstMember.c lstNext.c lstOpen.c lstRemove.c lstReplace.c lstSucc.c

UNUSEDOK = $(SRCFILES)

export DOINSTALL=yes
export INSTALLPREFIX=usr/bin

EXTRAINC = -I.
include $(TOP)/GNUmakefile.global
