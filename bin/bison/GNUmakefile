TOP = 		../..
PROG = 		bison
INSTALLPREFIX=	/usr/local/bin

DEFS =		-DSTDC_HEADERS=1 -DHAVE_STRING_H=1 -DHAVE_STDLIB_H=1 \
		-DHAVE_MEMORY_H=1 -DHAVE_ALLOCA_H=1 -DHAVE_ALLOCA=1 \
		-DHAVE_STRERROR=1

# names of parser files
PFILE = 	bison.simple
PFILE1 = 	bison.hairy
datadir =	/usr/local/share

PFILES = 	-DXPFILE=\"$(datadir)/$(PFILE)\" \
		-DXPFILE1=\"$(datadir)/$(PFILE1)\"

DEFS+= 		$(PFILES)


WARNOK =	$(SRCFILES)

SRCFILES =	LR0.c allocate.c closure.c conflicts.c derives.c \
		files.c getargs.c gram.c lalr.c lex.c main.c nullable.c \
		output.c print.c reader.c reduce.c symtab.c warshall.c \
		version.c getopt.c getopt1.c

DOINSTALL = 	yes

EXTRACLN =	bison.s1

include $(TOP)/GNUmakefile.global

install::
	$(RM) -f bison.s1
	sed -e "/^#line/ s|bison|$(datadir)/bison|" < ./$(PFILE) > bison.s1
	$(INSTALL) -d $(EXODEST)/usr/local/share
	$(INSTALL) -c -m 0644 bison.s1 $(EXODEST)$(datadir)/$(PFILE)
	$(INSTALL) -c -m 0644 $(PFILE1) $(EXODEST)$(datadir)/$(PFILE1)







