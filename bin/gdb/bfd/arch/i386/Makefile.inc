
SRCS+=		netbsd386.c cpu-i386.c

CFLAGS+=	-DNETBSD_CORE

VECTORS=	-DDEFAULT_VECTOR=netbsd_386_vec \
		-DSELECT_ARCHITECTURES=bfd_i386_arch \
		-DSELECT_VECS='&netbsd_386_vec'


