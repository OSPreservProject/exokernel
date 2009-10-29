
TOP = ../..

STATIC = 1
EXTRAINC = -I$(CURDIR)/.. -I$(TOP)/lib/libexos
LIB = libalfs.a
SRCFILES = alfs_alloc.c alfs_syscalls.c alfs_buffer.c \
           buffer_tab.c alfs_defaultcache.c alfs_inode.c \
           name_cache.c alfs_dinode.c alfs_path.c protection.c \
           alfs_dir.c alfs_rdwr.c

SUBDIRS = alfssep alfsemb
include $(TOP)/GNUmakefile.global

