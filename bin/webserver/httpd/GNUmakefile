
TOP = ../../..
PROG = cheetah
EXTRATRG = foobar
DEFS = -DPRINTF_LEVEL=1
SRCFILES = web_server.c \
	web_alias.c \
	web_config.c \
	web_error.c \
	web_header.c \
	web_log.c \
	web_datetime.c \
	buffer_tab.c \
	web_sepfs.c \
	#name_cache.c \

#LIBS = $(TOP)/lib/xio/obj/libxio.a $(TOP)/lib/libc/obj/libcx.a
#LIBS += $(TOP)/lib/alfs/libalfssep.a
SUBDIRS = webswamp \
	httpd2 \
	#test_datetime \
	#webdemo \

export DOINSTALL=yes
export INSTALLPREFIX=bin

PREORDER = yes
EXTRAINC = -I$(TOP)/lib/libexos -I$(TOP)/lib
include $(TOP)/GNUmakefile.global

install::
	$(INSTALL) -d $(EXODEST)/etc
	$(INSTALL) -c -m 0444 httpd.conf $(EXODEST)/etc/
	$(INSTALL) -c -m 0444 httpd2.conf $(EXODEST)/etc/

clean::
	rm -f httpd2/web_server.c
	rm -f httpd2/web_alias.c
	rm -f httpd2/web_datetime.c
	rm -f httpd2/web_config.c
	rm -f httpd2/web_error.c
	rm -f httpd2/web_header.c
	rm -f httpd2/web_log.c

foobar:
	ln -sf ../web_server.c httpd2/
	ln -sf ../web_alias.c httpd2/
	ln -sf ../web_datetime.c httpd2/
	ln -sf ../web_config.c httpd2/
	ln -sf ../web_error.c httpd2/
	ln -sf ../web_header.c httpd2/
	ln -sf ../web_log.c httpd2/

