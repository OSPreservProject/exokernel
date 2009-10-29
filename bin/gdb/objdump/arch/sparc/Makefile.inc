
SRCS+=		sparc-pinsn.c sparc-opc.c

CFLAGS+=	-DSELECT_ARCHITECTURES=bfd_arch_sparc \
		-DPRINT_INSN=print_insn_sparc


