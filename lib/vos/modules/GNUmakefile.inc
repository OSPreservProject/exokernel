
TOP = ../../..
VOS = $(TOP)/lib/vos


VPATH += $(VOS)/modules
SRCFILES += spipe.c prd.c

VPATH += $(VOS)/modules/fsrv
SRCFILES += fsrv.c

VPATH += $(VOS)/modules/udp
SRCFILES += udp_socket.c udp_recvfrom.c udp_init.c udp_sendto.c


VPATH += $(VOS)/modules/console
SRCFILES += console.c fillw.S


VPATH += $(VOS)/modules/ports
SRCFILES += ports.c


include $(TOP)/lib/vos/modules/xio/GNUmakefile.inc


