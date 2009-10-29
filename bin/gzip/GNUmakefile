
TOP = ../..
PROG = gzip
SRCFILES = gzip.c zip.c deflate.c trees.c bits.c unzip.c inflate.c util.c \
	crypt.c lzw.c unlzw.c unlzh.c unpack.c getopt.c
DEFS+=-DSTDC_HEADERS=1 -DHAVE_UNISTD_H=1 -DDIRENT=1

export DOINSTALL=yes
export INSTALLPREFIX=usr/bin

EXTRAINC = -I../../lib/libc
include $(TOP)/GNUmakefile.global
