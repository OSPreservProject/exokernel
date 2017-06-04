
TOP = ../..

DOINSTALL=yes
EXTRATRG=libgcc.a
EXTRAINSTALL=libgcc.a
EXTRAINSTALLPREFIX=vos

include $(TOP)/GNUmakefile.global

CP=cp
CROSS=

libgcc.a:
	$(CP) -f libgcc.a-raw libgcc.a
	ranlib libgcc.a
	#$(CROSS)/ranlib libgcc.a
