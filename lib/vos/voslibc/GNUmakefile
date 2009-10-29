
TOP = ../../..

DOINSTALL=yes
VOS = $(TOP)/lib/vos
OBJDIR = $(VOS)/voslibc/obj
DEFS=-DFLOATING_POINT=1 -DNOTEXOS 
EXTRAWFLAGS+=-Wno-uninitialized -Wno-error

STATIC=1
LIB=libc.a 

include $(VOS)/voslibc/stdio/GNUmakefile.inc
include $(VOS)/voslibc/string/GNUmakefile.inc
include $(VOS)/voslibc/locale/GNUmakefile.inc
include $(VOS)/voslibc/stdlib/GNUmakefile.inc
include $(VOS)/voslibc/quad/GNUmakefile.inc
include $(VOS)/voslibc/termios/GNUmakefile.inc
include $(VOS)/voslibc/gen/GNUmakefile.inc
include $(VOS)/voslibc/compat-43/GNUmakefile.inc

VPATH += $(VOS)/voslibc/misc
SRCFILES += util.c


include $(TOP)/GNUmakefile.global


