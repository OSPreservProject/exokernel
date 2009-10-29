TOP = ../..

all::
	@echo -n

install::
	$(INSTALL) -d -m 755 $(ROOT)/disk/av0
	$(INSTALL) -d -m 755 $(ROOT)/home/zw0
	$(INSTALL) -d -m 755 $(ROOT)/etc
	$(INSTALL) -d -m 755 $(ROOT)/var
	$(INSTALL) -d -m 777 $(ROOT)/tmp
	$(INSTALL) -d -m 777 $(ROOT)/var/tmp
	$(INSTALL) -d -m 777 $(ROOT)/usr/tmp
	$(INSTALL) -d -m 755 $(ROOT)/var/run
	$(INSTALL) -d -m 755 $(ROOT)/var/log
	$(INSTALL) -d -m 755 $(ROOT)/usr
	$(INSTALL) -d -m 755 $(ROOT)/usr/bin
	$(INSTALL) -d -m 755 $(ROOT)/usr/libexec
	$(INSTALL) -d -m 755 $(ROOT)/usr/lib
	$(INSTALL) -d -m 755 $(ROOT)/usr/mdec
	$(INSTALL) -d -m 755 $(ROOT)/usr/X11R6
	$(INSTALL) -d -m 755 $(ROOT)/usr/X11R6/lib
	$(INSTALL) -d -m 755 $(ROOT)/usr/X11R6/lib/X11
	$(INSTALL) -d -m 755 $(ROOT)/usr/X11R6/lib/X11/locale
	$(INSTALL) -d -m 755 $(ROOT)/usr/X11R6/lib/X11/locale/C
	$(INSTALL) -d -m 755 $(ROOT)/usr/local
	$(INSTALL) -d -m 755 $(ROOT)/usr/local/bin
	$(INSTALL) -d -m 755 $(ROOT)/usr/sbin
	$(INSTALL) -d -m 755 $(ROOT)/usr/share
	$(INSTALL) -d -m 755 $(ROOT)/usr/share/mk
	$(INSTALL) -d -m 755 $(ROOT)/usr/share/misc
	$(INSTALL) -d -m 755 $(ROOT)/usr/share/man/cat1
	$(INSTALL) -d -m 755 $(ROOT)/usr/cffs
	$(INSTALL) -d -m 755 $(ROOT)/usr/src
	$(INSTALL) -d -m 755 $(ROOT)/usr/include
	$(INSTALL) -d -m 755 $(ROOT)/home
	$(INSTALL) -d -m 755 $(ROOT)/home/benjie
	$(INSTALL) -d -m 755 $(ROOT)/home/cblake
	$(INSTALL) -d -m 755 $(ROOT)/mnt
	$(INSTALL) -d -m 755 $(ROOT)/cvs
	$(RM) $(ROOT)/bin/vi; ln -s nvi $(ROOT)/bin/vi

# 	$(INSTALL) -d -m 755 $(ROOT)/usr/share/nls
#	$(INSTALL) -d -m 755 $(ROOT)/usr/share/nls/C
#	$(INSTALL) -d -m 755 $(ROOT)/usr/X11R6/lib/X11/locale/iso8859-1
#	$(INSTALL) -d -m 755 $(ROOT)/usr/X11R6/lib/X11/app-defaults


include $(TOP)/GNUmakefile.global
