TOP = ../..

all::
	@echo -n

export ROOT = $(EXODEST)

install::
	$(INSTALL) -c -m 644 shells $(ROOT)/etc
	$(INSTALL) -c -m 644 ttys $(ROOT)/etc
	$(INSTALL) -c -m 644 csh.cshrc $(ROOT)/etc
	$(INSTALL) -c -m 644 encoding.types $(ROOT)/etc
	$(INSTALL) -c -m 644 gettytab $(ROOT)/etc
	$(INSTALL) -c -m 644 group $(ROOT)/etc
	$(INSTALL) -c -m 644 hosts $(ROOT)/etc
	$(INSTALL) -c -m 644 inetd.conf $(ROOT)/etc
	$(INSTALL) -c -m 644 localtime $(ROOT)/etc
	$(INSTALL) -c -m 644 magic $(ROOT)/etc
	$(INSTALL) -c -m 644 master.passwd $(ROOT)/etc
	$(INSTALL) -c -m 644 mime.types $(ROOT)/etc
	$(INSTALL) -c -m 644 passwd $(ROOT)/etc
	$(INSTALL) -c -m 644 protocols $(ROOT)/etc
	$(INSTALL) -c -m 644 pwd.db $(ROOT)/etc
	$(INSTALL) -c -m 644 rebuild_passwd $(ROOT)/etc
	$(INSTALL) -c -m 644 resolv.conf $(ROOT)/etc
	$(INSTALL) -c -m 644 services $(ROOT)/etc
	$(INSTALL) -c -m 644 spwd.db $(ROOT)/etc
	$(INSTALL) -c -m 755 sboot $(ROOT)/etc
	$(INSTALL) -c -m 755 rc.local $(ROOT)/etc

include $(TOP)/GNUmakefile.global
