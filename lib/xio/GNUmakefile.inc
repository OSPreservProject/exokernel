
LIBXIO = $(TOP)/lib/xio

EXTRAINC += -I$(LIBXIO) -I$(TOP)/lib/libc -I$(TOP)/lib -I$(TOP)/lib/libexos -DPRINTF_LEVEL=-1

VPATH += $(TOP)/lib/xio
SRCFILES += xio_tcpbuffer.c xio_tcp_demux.c xio_tcp_timewait.c \
	xio_tcp_waitfor.c xio_tcp_handlers.c xio_tcp_stats.c xio_tcpsocket.c \
	exos_net_wrap.c exos_tcpsocket.c dpf-ir.c 

	#socknet_net_wrap.c sock_tcp.c \


