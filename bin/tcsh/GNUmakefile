TOP = ../..

PROG = 		tcsh
DOINSTALL=	yes
INSTALLPREFIX=	/usr/local/bin


WARNOK= 	tw.parse.c


DEFS +=	 	-DHAVENOUTMP

SRCFILES = 	sh.c sh.dir.c sh.dol.c sh.err.c sh.exec.c sh.char.c sh.exp.c \
		sh.func.c sh.glob.c sh.hist.c sh.init.c sh.lex.c sh.misc.c  \
		sh.parse.c sh.print.c sh.proc.c sh.sem.c sh.set.c sh.time.c \
		glob.c mi.termios.c ma.setp.c vms.termcap.c tw.help.c \
		tw.init.c tw.parse.c tw.spell.c tw.comp.c ed.chared.c \
		ed.refresh.c ed.screen.c ed.init.c ed.inputl.c ed.defns.c \
		ed.xmap.c ed.term.c tc.alloc.c tc.bind.c tc.const.c tc.defs.c \
		tc.disc.c tc.func.c tc.os.c tc.printf.c tc.prompt.c \
		tc.sched.c tc.sig.c tc.str.c tc.vers.c tc.who.c

EXTRAOBJ= 	$(TOP)/lib/libtermlib/obj/libtermlib.a

EXTRACLN=	gethost

include $(TOP)/GNUmakefile.global

WFLAGS += -Wno-error


