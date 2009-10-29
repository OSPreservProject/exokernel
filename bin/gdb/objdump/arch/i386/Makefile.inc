
SRCS+=		i386-pinsn.c

CFLAGS+=	-DSELECT_ARCHITECTURES=bfd_arch_i386 \
		-DPRINT_INSN=print_insn_i386


