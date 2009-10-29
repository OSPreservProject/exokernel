TOP=		../../..

PROG=		objdump
DOINSTALL=	yes
INSTALLPREFIX=	/usr/bin

LIBBFD =  	$(CURDIR)/../bfd/obj/libbfd.a
LIBIBERTY = 	$(CURDIR)/../libiberty/obj/libiberty.a


EXTRAINC=	-I./arch/i386 -I../bfd \
		-I../bfd/arch/i386 -I../include \
		-I. 

DEFS+=		-DVERSION=\"2.0\" \
		-DSELECT_ARCHITECTURES=bfd_arch_i386 \
		-DPRINT_INSN=print_insn_i386

WARNOK=		objdump.c bucomm.c filemode.c i386-pinsn.c

SRCFILES=	objdump.c bucomm.c filemode.c version.c

EXTRAOBJ=	$(LIBBFD) $(LIBIBERTY)

VPATH +=	$(CURDIR)/arch/i386
SRCFILES +=	i386-pinsn.c

include  $(TOP)/GNUmakefile.global