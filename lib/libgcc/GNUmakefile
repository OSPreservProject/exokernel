
TOP = ../..

DOINSTALL=yes
EXTRATRG=libgcc.a
EXTRAINSTALL=libgcc.a
EXTRAINSTALLPREFIX=vos

include $(TOP)/GNUmakefile.global

CP=cp
CROSS=/opt/openbsd-cross/bin

libgcc.a:
	$(CP) -f libgcc.a-raw libgcc.a
	$(CROSS)/ranlib libgcc.a
