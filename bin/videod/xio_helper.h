void xio_init (xio_dmxinfo_t **dmxinfo, xio_twinfo_t **twinfo, 
		xio_nwinfo_t **nwinfo);
void xio_listen (int port, xio_nwinfo_t *nwinfo, 
		 xio_dmxinfo_t *dmxinfo);
struct ae_recv *xio_get_next_packet (xio_nwinfo_t *nwinfo,
				     xio_dmxinfo_t *dmxinfo);
struct ae_recv *xio_poll_packet (xio_nwinfo_t *nwinfo, xio_dmxinfo_t *dmxinfo);
void xio_handle_next_packet (struct tcb **tcb, xio_nwinfo_t *nwinfo,
			     xio_dmxinfo_t *dmxinfo);
struct tcb *xio_start_accept (struct ae_recv *packet, 
			       xio_nwinfo_t *nwinfo, 
			       xio_dmxinfo_t *dmxinfo,
			      struct tcb *tcb);
int xio_finish_accept (struct tcb *tcb, xio_nwinfo_t *nwinfo, 
		       xio_dmxinfo_t *dmxinfo);
