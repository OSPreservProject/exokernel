
SRCS+=		m68k-pinsn.c

CFLAGS+=	-DSELECT_ARCHITECTURES=bfd_arch_m68k \
		-DPRINT_INSN=print_insn_m68k


