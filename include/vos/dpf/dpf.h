
#ifndef __VOS_DPF_H__
#define __VOS_DPF_H__

/*
 * Below are generic dpf filters for networking
 */

/*
 * dpf_bind_udpfilter_priv: bind an udp filter for the given process. Returns
 * filter ID if successful. Returns -1 otherwise. Errno is set to 
 *
 *   V_ADDRNOTAVAIL: unable to insert filter
 */
extern int
dpf_bind_udpfilter(unsigned short p, unsigned char *ip_src, 
                   int ring_id, u_int envid);


/*
 * dpf_bind_tcpfilter_priv: bind a tcp filter for the given process. Returns
 * filter ID if successful. Returns -1 otherwise. Errno is set to 
 *
 *   V_ADDRNOTAVAIL: unable to insert filter
 */
extern int 
dpf_bind_tcpfilter(u_short localport, int remoteport, int ringid, u_int envid);


/*
 * dpf_delete_binding: removes a dpf filter for an environment. Returns 0 if
 * successful. Negative values (kernel errno from sys_dpf_delete) otherwise.
 */
extern int
dpf_delete_binding(u_int fid, u_int envid);


#endif

