TOP=		../../..

PROG=		gdb
DOINSTALL=	yes
INSTALLPREFIX=	/usr/bin

LIBBFD =  	$(CURDIR)/../bfd/obj/libbfd.a
LIBREADLINE =	$(CURDIR)/../readline/obj/libreadline.a
LIBIBERTY = 	$(CURDIR)/../libiberty/obj/libiberty.a


EXTRAINC=	-I./arch/i386 -I../bfd \
		-I../readline -I../include \
		-I. 

DEFS+=		-DHOST_CANONICAL=\"i386-openbsd\" \
		-DTARGET_CANONICAL=\"i386-openbsd\"

DEFS+=		-DNO_MMALLOC -DHAVE_TERMIOS
#DEFS+=		-DKERNEL_DEBUG



#WARNOK=	main.c source.c breakpoint.c maint.c buildsym.c corelow.c \
		mem-break.c stack.c c-exp.c dbxread.c symmisc.c inflow.c \
		symtab.c objfiles.c ch-exp.c 

WARNOK=		$(SRCFILES)


SRCFILES=	blockframe.c core.c gdbtypes.c main.c source.c		\
		breakpoint.c maint.c  stabsread.c			\
		buildsym.c corelow.c kcorelow.c mem-break.c stack.c	\
		c-exp.c cp-valprint.c minsyms.c symfile.c		\
		c-lang.c dbxread.c elfread.c infcmd.c symmisc.c		\
		c-typeprint.c dcache.c inflow.c symtab.c		\
		c-valprint.c demangle.c infptrace.c objfiles.c target.c	\
		ch-exp.c infrun.c parse.c dwarfread.c thread.c		\
		ch-lang.c inftarg.c printcmd.c typeprint.c		\
		ch-typeprint.c environ.c init.c utils.c			\
		ch-valprint.c eval.c  language.c regex.c  valarith.c	\
		exec.c  m2-exp.c remote-utils.c valops.c		\
		command.c expprint.c m2-lang.c remote.c valprint.c	\
		complaints.c findvar.c m2-typeprint.c ser-unix.c values.c\
		copying.c fork-child.c m2-valprint.c serial.c version.c \
		dis-buf.c solib.c


EXTRAOBJ=	$(LIBBFD) $(LIBREADLINE) $(LIBIBERTY) \
		$(TOP)/lib/libterm/obj/libtermcap.a 

#EXTRAOBJ=	$(LIBKVM)

VPATH +=	$(CURDIR)/arch/i386
SRCFILES +=	i386-pinsn.c i386-tdep.c i386-dis.c i386b-nat.c

include  $(TOP)/GNUmakefile.global