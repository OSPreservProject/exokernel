
#include <stdio.h>
#include <xok/mmu.h>
#include <xok/sys_ucall.h>

#include <vos/proc.h>
#include <vos/cap.h>
#include <vos/uinfo.h>
#include <vos/ipc.h>
#include <vos/ipc-services.h>
#include <vos/vm.h>
#include <vos/vm-layout.h>
#include <vos/errno.h>
#include <vos/assert.h>

#include <dpf/dpf.h>

#include "../../modules/ports/ports.h"

char *progname = "portsd";


#define PORTS_BIND_UDP_IPCID MIN_APP_IPC_NUMBER+1
#define PORTS_BIND_TCP_IPCID PORTS_BIND_UDP_IPCID+1
#define PORTS_RESERVE_IPCID  PORTS_BIND_TCP_IPCID+1
#define PORTS_UNBIND_IPCID   PORTS_RESERVE_IPCID+1


#define RET2ARGS(r1, r2) \
{ \
  asm volatile ("\tmovl %0, %%edx" :: "g" (r2)); \
  return r1; \
}


static int 
bind_udp_ipch(int,int,int,int,u_int,void*) __attribute__ ((regparm (3)));
static int 
bind_tcp_ipch(int,int,int,int,u_int,void*) __attribute__ ((regparm (3)));
static int 
reserve_ipch(int,int,int,int,u_int,void*) __attribute__ ((regparm (3)));
static int 
unbind_ipch(int,int,int,int,u_int,void*) __attribute__ ((regparm (3)));


static int
bind_udp_ipch(int task, int arg1, int arg2, int ipctype, u_int envid, void* d)
{
  /* arg1: port; arg2: ip address; extra arg: ringid */
  int r;
  u_char ipsrc[4];
  u_short port = arg1;
  u_int ringid;
  
  if (arg1 < 0)
    RETERR(V_INVALID);
  
  if (ipctype == IPC_PCT)
    ringid = UAREA.u_ipc1_extra_args[0];
  else
  {
    struct ipc_msgring_ent* ringptr = (struct ipc_msgring_ent*)d;
    ringid = ((u_int*)(ringptr->space))[IPC_MSG_EXTRAARG_START_IDX];
  }

  /* XXX - check access privilege */

  memmove(&ipsrc[0], &arg2, 4);
  r = ports_bind_udp_priv(&port, ipsrc, ringid, envid);

  if (r < 0) /* looking for a port */
    RETERR(V_ADDRNOTAVAIL);

  RET2ARGS(r,port);
}


static int
bind_tcp_ipch(int task, int arg1, int arg2, int ipctype, u_int envid, void* d)
{
  /* arg1: srcport; arg2: dstport; extra arg: ringid */
  int r;
  u_short srcport = arg1;
  int dstport = arg2; 
  u_int ringid;

  if (arg2 > MAX_PORT)
    RETERR(V_INVALID);

  if (arg1 < 0)
    RETERR(V_INVALID);
  
  if (ipctype == IPC_PCT)
    ringid = UAREA.u_ipc1_extra_args[0];
  else
  {
    struct ipc_msgring_ent* ringptr = (struct ipc_msgring_ent*)d;
    ringid = ((u_int*)(ringptr->space))[IPC_MSG_EXTRAARG_START_IDX];
  }

  /* XXX - check access privilege */

  r = ports_bind_tcp_priv(&srcport, dstport, ringid, envid);

  if (r < 0) /* looking for a port */
    RETERR(V_ADDRNOTAVAIL);

  RET2ARGS(r,srcport);
}


static int
unbind_ipch(int task, int arg1, int arg2, int ipctype, u_int envid, void* d)
{
  /* arg1: port */
  if (arg1 <= 0 || arg1 > MAX_PORT)
    RETERR(V_INVALID);

  /* XXX - check access privilege */

  ports_unbind_priv(arg1);
  return 0;
}


static int 
reserve_ipch(int task, int arg1, int arg2, int ipctype, u_int envid, void* d)
{
  u_short port = arg1;
  int r;

  /* arg1: port */
  if (arg1 < 0 || arg1 > MAX_PORT)
    RETERR(V_INVALID);

  /* XXX - check access privilege */
  r = ports_reserve_priv(&port, envid);

  if (r != 0)
    RETERR(V_ADDRNOTAVAIL);

  RET2ARGS(r,port);
}


void
__init_start(void)
{
  int r, i;
  extern char *progname;

  uinfo_remap();

  r = vm_alloc_region(PORTS_REGION, PORTS_REGION_SZ, CAP_DPF, PG_U|PG_P|PG_W);

  if (r < 0 || r != PGNO(PORTS_REGION_SZ))
  {
    kprintf("%s: cannot initialize ports table, network programs may "
 	    "not be able to negotiate ports.\n", progname);
    exit(-1);
  }
  
  StaticAssert(sizeof(ports_table_t) <= PORTS_REGION_SZ);

  for(i=0; i< PGNO(PORTS_REGION_SZ); i++)
  {
    r = va2ppn(PORTS_REGION+i*NBPG);
    uinfo->ports_where[i] = r;
  }
  
  bzero(ports_table,sizeof(ports_table_t));

  for(i=0; i<MAX_PORT+1; i++)
    ports_table->ports[i] = 0;

  ports_table->ipc_taskid_bind_udp = PORTS_BIND_UDP_IPCID;
  ports_table->ipc_taskid_bind_tcp = PORTS_BIND_TCP_IPCID;
  ports_table->ipc_taskid_unbind = PORTS_UNBIND_IPCID;
  ports_table->ipc_taskid_reserve = PORTS_RESERVE_IPCID;
  ports_table->port_idx = PRIV_PORT;
  yieldlock_reset(&(ports_table->lock));
  ports_table->owner_envid = sys_geteid();
}


int 
main()
{
  if (ports_remap() < 0)
  {
    printf("%s: warning: cannot remap ports table\n",progname);
    exit(-1);
  }

  ipc_register_handler(PORTS_BIND_UDP_IPCID, bind_udp_ipch);
  ipc_register_handler(PORTS_BIND_TCP_IPCID, bind_tcp_ipch);
  ipc_register_handler(PORTS_RESERVE_IPCID,  reserve_ipch);
  ipc_register_handler(PORTS_UNBIND_IPCID,   unbind_ipch);
  
  env_suspend();
  return 0;
}


