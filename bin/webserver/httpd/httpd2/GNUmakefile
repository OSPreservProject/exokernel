
TOP = ../../../..
PROG = httpd2
#OBJDIR = obj
#EXTRAINC = -DPRINTF_LEVEL=1 -DHIGHLEVEL
SRCFILES = web_server.c \
	web_alias.c \
	web_config.c \
	web_error.c \
	web_header.c \
	web_log.c \
	web_datetime.c \
	sock_tcp.c \
	#name_cache.c \

#LIBS = $(TOP)/lib/alfs/libalfssep.a $(TOP)/lib/libc/obj/libc.a
#LIBS += $(TOP)/lib/xio/obj/libxio.a
#LIBC =
#LIBC = $(TOP)/lib/xio/obj/libxio.a $(TOP)/lib/libc/obj/libc.a

export DOINSTALL=yes
export INSTALLPREFIX=bin

PREORDER = yes
EXTRAINC = -I$(TOP)/lib/libexos -I$(TOP)/lib -I.. -DHIGHLEVEL
include $(TOP)/GNUmakefile.global

