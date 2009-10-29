
LIBXIO = $(TOP)/lib/vos/modules/xio

EXTRAINC += -I$(LIBXIO) 

VPATH += $(TOP)/lib/vos/modules/xio
SRCFILES += xio_tcpbuffer.c xio_tcp_demux.c xio_tcp_timewait.c \
	xio_tcp_waitfor.c xio_tcp_handlers.c xio_tcp_stats.c \
	xio_tcpsocket.c vos_net_wrap.c vos_tcpsocket.c


