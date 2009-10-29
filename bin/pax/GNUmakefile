
TOP = ../..
PROG = pax
SRCFILES = ar_io.c ar_subs.c buf_subs.c cache.c cpio.c file_subs.c ftree.c\
        gen_subs.c getoldopt.c options.c pat_rep.c pax.c sel_subs.c tables.c\
        tar.c tty_subs.c
UNUSEDOK = ar_io.c ar_subs.c buf_subs.c cache.c cpio.c file_subs.c ftree.c\
        gen_subs.c getoldopt.c options.c pat_rep.c pax.c sel_subs.c tables.c\
        tar.c tty_subs.c

export DOINSTALL=yes
export INSTALLPREFIX=bin

include $(TOP)/GNUmakefile.global

install::
	$(LN) -f $(EXODEST)/$(INSTALLPREFIX)/$(PROG) \
		$(EXODEST)/$(INSTALLPREFIX)/tar
	$(LN) -f $(EXODEST)/$(INSTALLPREFIX)/$(PROG) \
		$(EXODEST)/$(INSTALLPREFIX)/cpio
