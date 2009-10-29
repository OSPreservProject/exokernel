TOP = ../..
PROG = less
DEFS=-Dlint

EXTRAINC = -I../../lib/libc -I.

SRCFILES =	main.c screen.c brac.c ch.c charset.c cmdbuf.c \
	command.c decode.c edit.c filename.c forwback.c \
	help.c ifile.c input.c jump.c line.c linenum.c \
	lsystem.c mark.c optfunc.c option.c opttbl.c os.c \
	output.c position.c prompt.c search.c signal.c \
	tags.c ttyin.c version.c  

EXTRAOBJ=  $(TOP)/lib/libterm/obj/libtermcap.a \
        $(TOP)/lib/libcurses/obj/libcurses.a

export DOINSTALL=yes

include $(TOP)/GNUmakefile.global
WFLAGS = 
