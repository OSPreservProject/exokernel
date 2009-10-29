TOP = ../..
STATIC=1
PROG = rconsoled.b.gdb
SRCFILES = rconsoled.c setup_net.c checkkern.c nfs_root.c
EXTRATRG = rconsoled.b.s rconsoled.b.gdb
EXTRAOBJ = ../../lib/libc/obj/libc.a 

ifdef NFS_ROOT_PATH
DEFS += -DMY_NFS_ROOT_PATH=$(NFS_ROOT_PATH)
endif

ifdef NFS_ROOT_HOST
DEFS += -DMY_NFS_ROOT_HOST=$(NFS_ROOT_HOST)
endif

LIBS = $(TOP)/lib/libc/obj/libc.a

# there are two other options inside files:
#  arpd_res.c ARINGBUF to use pktrings
VPATH += $(TOP)/bin/arpd
SRCFILES += arpd.c vos_compat.c

VPATH += $(TOP)/bin/loadkern
SRCFILES += loadkern.c exec.c

EXTRAINC = -I$(TOP)/lib/libexos
# maybe dave can make this nicer
EXTRAINC += -DSTARTUPSHELL="\"/$(USER)/bin/hsh\"" -DRCLOCAL=\""/etc/rc.local\""

INITPROG=1
include $(TOP)/GNUmakefile.global


install:: all
