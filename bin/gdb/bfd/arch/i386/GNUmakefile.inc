
SRCFILES+=	netbsd386.c cpu-i386.c

DEFS+=		-DNETBSD_CORE

DEFS+=		-DDEFAULT_VECTOR=netbsd_386_vec \
		-DSELECT_ARCHITECTURES=bfd_i386_arch \
		-DSELECT_VECS='&netbsd_386_vec'


