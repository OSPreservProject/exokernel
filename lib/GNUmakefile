#
# Using += for easy commenting out.
#
TOP = ..
SUBDIRS += libgcc
SUBDIRS += libc
SUBDIRS += csu
#SUBDIRS += ash	# ashes seem to be being phased out
#SUBDIRS += ffs # no one uses it, so don't bother compiling for now
SUBDIRS += alfs
ifndef STATIC
SUBDIRS += $(TOP)/bin/shlib
endif	# not STATIC
SUBDIRS += libcurses
SUBDIRS += libterm
SUBDIRS += libtermlib
SUBDIRS += libl
SUBDIRS += libedit
SUBDIRS += libtelnet
SUBDIRS += libutil
SUBDIRS += libm
SUBDIRS += libg
SUBDIRS += libz
SUBDIRS += libpcap
SUBDIRS += libwafer
SUBDIRS += vos
include $(TOP)/GNUmakefile.global
