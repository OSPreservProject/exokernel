TOP=		../../..

LIB=		libbfd.a
OBJDIR= 	obj
STATIC= 	1

WARNOK=		archive.c coffgen.c libbfd.c targets.c ecoff.c netbsd-core.c

EXTRAINC=	-I../../include/ \
		-I../arch/i386 -I../ \
		-I../$(TOP)/lib/libc

DEFS+=		-DNETBSD_CORE
DEFS+=		-DDEFAULT_VECTOR=netbsd_386_vec \
                -DSELECT_ARCHITECTURES=bfd_i386_arch \
                -DSELECT_VECS='&netbsd_386_vec'

SRCFILES=	archive.c archures.c bfd.c cache.c coffgen.c core.c ctor.c \
		format.c init.c libbfd.c opncls.c reloc.c seclet.c section.c \
		syms.c targets.c ecoff.c elf.c srec.c aout32.c \
		stab-syms.c netbsd-core.c

VPATH+=		$(CURDIR)/arch/i386
SRCFILES+=	cpu-i386.c netbsd386.c


include $(TOP)/GNUmakefile.global


