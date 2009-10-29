
TOP = ../..
EXTRATRG = foobar
SUBDIRS = embdir-test \
          initfs \
          initfs2 \
          namecache-test \
          readfile \
          scanemb \
          scansep \
          simple-test \
          simple-test2 \
          small \
          small2 \
          fsbench \
          fsbench2 \
          rmalfs \
          rmalfs2 \

export DOINSTALL=yes
export INSTALLPREFIX=usr/alfs


PREORDER = yes
include $(TOP)/GNUmakefile.global

foobar:
	ln -fs ../initfs/initfs.c initfs2/initfs2.c
	ln -fs ../simple-test/simple-test.c simple-test2/simple-test2.c
	ln -fs ../small/small.c small2/small2.c
	ln -fs ../scansep/scansep.c rmalfs/scansep.c
	ln -fs ../scanemb/scanemb.c rmalfs2/scanemb.c
	ln -fs ../fsbench/dtangbm.h fsbench2/dtangbm.h
	ln -fs ../fsbench/exo-adapt.h fsbench2/exo-adapt.h
	ln -fs ../fsbench/fsbench.c fsbench2/fsbench2.c
	ln -fs ../fsbench/functions.h fsbench2/functions.h
	ln -fs ../fsbench/locality.c fsbench2/locality.c
	ln -fs ../fsbench/metadataFFS.c fsbench2/metadataFFS.c
	ln -fs ../fsbench/metadataSeek.c fsbench2/metadataSeek.c
	ln -fs ../fsbench/utility.c fsbench2/utility.c
	ln -fs ../fsbench/utility.h fsbench2/utility.h
