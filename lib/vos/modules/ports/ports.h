
#ifndef __VOS_MODULE_PORTS_H__
#define __VOS_MODULE_PORTS_H__

#include <xok/types.h>
#include <vos/locks.h>


#define MAX_PORT 65535 /* size of unsigned short */
#define PRIV_PORT 1024 /* priviledged port number */

typedef struct 
{
  u_int owner_envid;
  yieldlock_t lock;
  u_short ipc_taskid_bind_udp;
  u_short ipc_taskid_bind_tcp;
  u_short ipc_taskid_unbind;
  u_short ipc_taskid_reserve;
  u_short port_idx;
  u_int   ports[MAX_PORT+1];
} ports_table_t;

extern ports_table_t *ports_table;


extern void ports_init();
extern void ports_reset();


/*
 * ports_remap: remap PORTS_REGION writable. Returns 0 if successful.
 * Otherwise, returns -1. Errno is set to V_NOMEM.
 */
extern int ports_remap();

/*
 * ports_reserve_priv: reserve the port given for the enviornment given. If
 * *port is 0, find a new port. Returns 0 if successful. If ports already
 * taken, return the envid of the environment owning the port. Returns -1 if
 * no free port can be found. Errno is set to V_ADDRNOTAVAIL.
 */
extern int ports_reserve_priv(u_short *port, u_int envid);

/*
 * ports_unbind: unbinds a port number in the ports table. 
 */
extern void ports_unbind_priv(u_short port);

/*
 * ports_bind_udp_priv: creates a dpf binding for the environment (envid).
 * Allocate a port if *port is 0. Returns filter id if successful. Returns -1
 * otherwise. Errno is set to V_ADDRNOTAVAIL.
 */
extern int 
ports_bind_udp_priv(u_short *port, u_char *ipsrc, int ringid, u_int envid);

/*
 * ports_bind_tcp_priv: creates a dpf binding for the environment (envid).
 * For dpf filter, destination port in packet is ignored if remoteport is -1.
 * Allocate a free port if *localport is 0.  Returns filter id if successful.
 * Returns -1 otherwise.  Errno is set to V_ADDRNOTAVAIL.
 */
extern int 
ports_bind_tcp_priv(u_short *localport, int remoteport, int ringid, u_int envid);


/* The following four calls are unprivileged versions of the above four calls.
 * They make IPC calls to the ports daemon. There can only be one pending IPC
 * calls for each operation (i.e. 1 for bind_udp, 1 for bind_tcp, etc).
 */

/*
 * ports_bind_udp: bind an udp port for the environment given. If *port is
 * zero, select a port first. If the process does not have privilege to bind
 * ports, ipc to ports daemon. Returns the dpf filter id of the port. If a new
 * port is created, the port number is stored in *port. Returns -1 if fails.
 * Errno is from union of those defined by pct_sleepwait and V_ADDRNOTAVAIL
 * and V_INVALID (*port given bad).
 */
extern int ports_bind_udp(u_short *port, u_char *ipsrc, int ringid);

/*
 * ports_unbind: unbinds the port given. Either unbind directly from the ports
 * table if priviledged, or make an IPC call to portsd. Returns 0.
 */
extern int ports_unbind(u_short port);

/*
 * ports_bind_tcp: bind a tcp port for the environment given. If *localport is
 * zero, select a port first. If the process does not have privilege to bind
 * ports, ipc to ports daemon. For dpf filter, destination port in packet is
 * ignored if remoteport is -1.  Returns the dpf filter id of the port. If a
 * new port is created, the port number is stored in *localport. Returns -1 if
 * fails.  Errno is from union of those defined by pct_sleepwait and
 * V_ADDRNOTAVAIL and V_INVALID (*localport given is bad).
 */
extern int ports_bind_tcp(u_short *localport, int remoteport, int ringid);

/*
 * ports_reserve: reserve the port given. If *port is 0, obtain a free port.
 * If unprivileged, make ipc call to ports daemon. Return values equal to that
 * of ports_reserve_priv.
 */
extern int ports_reserve(u_short *port);


#endif


