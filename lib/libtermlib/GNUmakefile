
TOP = ../..
OBJDIR = obj
STATIC=1
EXTRAINC = -I$(CURDIR) -I$(CURDIR)/../libc 
LIB = libtermlib.a

HDRS = term.h
EXTRACLN =	$(CURDIR)/$(HDRS) $(CURDIR)/binaries.h $(CURDIR)/captoidx.c \
		$(CURDIR)/infotoidx.c $(CURDIR)/mkinfo \
		$(CURDIR)/boolnames.c $(CURDIR)/boolcodes.c $(CURDIR)/boolfnames.c \
		$(CURDIR)/numnames.c $(CURDIR)/numcodes.c  $(CURDIR)/numfnames.c \
		$(CURDIR)/strnames.c  $(CURDIR)/strcodes.c  $(CURDIR)/strfnames.c

UNINITOK =
UNUSEDOK =	captoinfo.c def_prog_mode.c def_shell_mode.c del_curterm.c \
		fillcap.c findcap.c findinfo.c getterm.c globals.c mvcur.c _mvcur.c \
		outc.c putp.c reset_prog_mode.c reset_shell_mode.c \
		restartterm.c setterm.c setupterm.c set_curterm.c tgetent.c \
		tgetflag.c tgetnum.c tgetstr.c tgoto.c tigetflag.c tigetnum.c \
		tigetstr.c tparm.c tputs.c tty_init.c \
		vidattr.c _vidattr.c \
		vidputs.c _vidputs.c \
		captoidx.c infotoidx.c \
		boolnames.c boolcodes.c boolfnames.c \
		numnames.c  numcodes.c  numfnames.c \
		strnames.c  strcodes.c  strfnames.c

SRCFILES=	captoinfo.c def_prog_mode.c def_shell_mode.c del_curterm.c \
		fillcap.c findcap.c findinfo.c getterm.c globals.c mvcur.c _mvcur.c \
		outc.c putp.c reset_prog_mode.c reset_shell_mode.c \
		restartterm.c setterm.c setupterm.c set_curterm.c tgetent.c \
		tgetflag.c tgetnum.c tgetstr.c tgoto.c tigetflag.c tigetnum.c \
		tigetstr.c tparm.c tputs.c tty_init.c \
		vidattr.c _vidattr.c \
		vidputs.c _vidputs.c \
		captoidx.c infotoidx.c \
		boolnames.c boolcodes.c boolfnames.c \
		numnames.c  numcodes.c  numfnames.c \
		strnames.c  strcodes.c  strfnames.c

export DOINSTALL=yes

include $(TOP)/GNUmakefile.global

$(SRCFILES:.c=.o):	$(HDRS) binaries.h

term.h:		mkcaps.sh term.h.head Capabilities term.h.tail
	sh $(CURDIR)/mkcaps.sh $(CURDIR)/term.h.head \
				$(CURDIR)/Capabilities \
				$(CURDIR)/term.h.tail > $(CURDIR)/term.h

boolnames.c boolcodes.c boolfnames.c \
numnames.c  numcodes.c  numfnames.c \
strnames.c  strcodes.c  strfnames.c \
captoidx.c infotoidx.c \
binaries.h:	mkinfo Capabilities
	./mkinfo $(CURDIR)/Capabilities

ifdef LINUX
mkinfo:	mkinfo.c
	$(NCC) -DLINUX -I$(CURDIR) -o mkinfo $(CURDIR)/mkinfo.c
else
mkinfo:	mkinfo.c
	$(NCC) -I$(CURDIR) -o mkinfo $(CURDIR)/mkinfo.c
endif
