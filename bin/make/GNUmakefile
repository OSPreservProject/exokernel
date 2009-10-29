
TOP = ../..
PROG = gmake

export DOINSTALL=yes
srcdir = .
# Which flavor of remote job execution support to use.
# The code is found in `remote-$(REMOTE).c'.
REMOTE = stub

# If your system doesn't have alloca, or the one provided is bad, define this.
ALLOCA = 
ALLOCA_SRC = $(srcdir)/alloca.c

# Common prefix for machine-independent installed files.
prefix = /usr/local
# Common prefix for machine-dependent installed files.
exec_prefix = ${prefix}

# Directory to install `make' in.
bindir = $(exec_prefix)/bin
# Directory to find libraries in for `-lXXX'.
libdir = $(exec_prefix)/lib
# Directory to search by default for included makefiles.
includedir = $(prefix)/include
# Directory to install the Info files in.
infodir = $(prefix)/info
# Directory to install the man page in.
mandir = $(prefix)/man/man$(manext)
# Number to put on the man page filename.
manext = 1
# Prefix to put on installed `make' binary file name.
binprefix =
# Prefix to put on installed `make' man page file name.
manprefix = $(binprefix)


DEFS += -DHAVE_CONFIG_H -DLIBDIR=\"$(libdir)\" -DINCLUDEDIR=\"$(includedir)\"

SRCFILES = $(srcdir)/commands.c $(srcdir)/job.c $(srcdir)/dir.c		\
       $(srcdir)/file.c $(srcdir)/getloadavg.c $(srcdir)/misc.c		\
       $(srcdir)/main.c $(srcdir)/read.c $(srcdir)/remake.c		\
       $(srcdir)/rule.c $(srcdir)/implicit.c $(srcdir)/default.c	\
       $(srcdir)/variable.c $(srcdir)/expand.c $(srcdir)/function.c	\
       $(srcdir)/vpath.c $(srcdir)/version.c				\
       $(srcdir)/remote-$(REMOTE).c					\
       $(srcdir)/ar.c $(srcdir)/arscan.c				\
       $(srcdir)/signame.c $(srcdir)/signame.h				\
       $(srcdir)/getopt.c $(srcdir)/getopt1.c $(srcdir)/getopt.h	\
       $(srcdir)/commands.h $(srcdir)/dep.h $(srcdir)/filedef.h		\
       $(srcdir)/job.h $(srcdir)/make.h $(srcdir)/rule.h		\
       $(srcdir)/variable.h $(ALLOCA_SRC) $(srcdir)/config.h.in

EXTRAINC = -I../../lib/libc -I.
include $(TOP)/GNUmakefile.global

WFLAGS=
