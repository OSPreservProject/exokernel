
/*
 * Copyright MIT 1999
 */

#ifndef __VOS_AE_ETHER_H__
#define __VOS_AE_ETHER_H__

/*
 * ae_eth_send: sends a packet on the netcard specified. Returns 0 if
 * successful. -1 if fails.
 */
extern int
ae_eth_send(void *d, int sz, int netcardno);

/*
 * ae_eth_sendv: sends scattered packet. Returns 0 if successful. -1 if fails.
 */
extern int
ae_eth_sendv(struct ae_recv *outgoing, int netcardno);


#endif

