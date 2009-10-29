
TOP = ../../..
EXTRAINC = -I$(CURDIR)/../.. -I$(TOP)/lib/libexos
DEFS = -DEMBEDINODES -DGROUPING #-DFORCE_DIRECTORY_COLLOCATION
STATIC = 1
LIB = ../libalfsemb.a

VPATH = ..
SRCFILES = alfs_alloc.c alfs_embdir.c alfs_embpath.c alfs_dinode.c \
           alfs_syscalls.c alfs_buffer.c buffer_tab.c \
           alfs_defaultcache.c alfs_inode.c name_cache.c \
           protection.c alfs_rdwr.c

include $(TOP)/GNUmakefile.global
