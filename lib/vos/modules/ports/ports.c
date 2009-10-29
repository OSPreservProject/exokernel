
#include <stdio.h>
#include <xok/mmu.h>

#include <vos/proc.h>
#include <vos/cap.h>
#include <vos/errno.h>
#include <vos/uinfo.h>
#include <vos/ipc.h>
#include <vos/vm-layout.h>
#include <vos/vm.h>
#include <vos/assert.h>
#include <vos/dpf/dpf.h>


#include "ports.h"
#define dprintf if (0) kprintf


ports_table_t *ports_table = (ports_table_t *)PORTS_REGION;


int
ports_map(u_int va, u_int errcode, u_int eflags, u_int eip)
{
  int r, n;
  Pte ptes[PGNO(PORTS_REGION_SZ)];

  StaticAssert(PORTS_REGION_SZ >= sizeof(ports_table_t));

  fault_handler_add(ports_map, PORTS_REGION, PORTS_REGION_SZ);

  if (uinfo->ports_where[0] == 0)
  {
    printf("ports table not mapped yet, skip\n");
    return -1;
  }

  for(n=0; n<PGNO(PORTS_REGION_SZ); n++)
    ptes[n] = (uinfo->ports_where[n] << PGSHIFT) |PG_U|PG_P|PG_SHARED;

  n=0;
  r = vm_self_insert_pte_range(CAP_USER, ptes,
                               PGNO(PORTS_REGION_SZ), PORTS_REGION,
			       &n, 0, 0L);
  if (r < 0 || n != PGNO(PORTS_REGION_SZ))
    return -1;
  return 0;
}


void
ports_init()
{
  if (ports_map(0,0,0,0) < 0)
  {
    printf("ports_init: cannot map ports_table!\n");
  }
}

  
int
ports_remap()
{
  int r, n;
  Pte ptes[PGNO(PORTS_REGION_SZ)];

  for(n=0; n<PGNO(PORTS_REGION_SZ); n++)
    ptes[n] = (uinfo->ports_where[n] << PGSHIFT) |PG_U|PG_P|PG_SHARED|PG_W;
 
  n=0;
  r = vm_self_insert_pte_range(CAP_DPF, ptes,
                               PGNO(PORTS_REGION_SZ), PORTS_REGION,
			       &n, 0, 0L);

  if (r < 0 || n != PGNO(PORTS_REGION_SZ))
  {
    printf("ports_remap: cannot remap ports_table!\n");
    RETERR(V_NOMEM);
  }
  return 0;
}



void
ports_unbind_priv(u_short port)
{
  yieldlock_acquire(&(ports_table->lock));
  ports_table->ports[port] = 0;
  yieldlock_release(&(ports_table->lock));
}



static int 
ports_get_priv(u_int envid)
{
  u_short cur;
  u_short uplim = MAX_PORT;

  yieldlock_acquire(&(ports_table->lock));
  cur = ports_table->port_idx;

repeat:
  while(ports_table->port_idx <= uplim)
  {
    if (ports_table->ports[ports_table->port_idx] == 0)
    {
      ports_table->ports[ports_table->port_idx] = envid;
      cur = ports_table->port_idx;
      yieldlock_release(&(ports_table->lock));
      return cur;
    }
    else
    {
      int holder = ports_table->ports[ports_table->port_idx];
      if (!env_alive(holder))
      {
        ports_table->ports[ports_table->port_idx] = envid;
        cur = ports_table->port_idx;
        yieldlock_release(&(ports_table->lock));
        return cur;
      }
    }
    ports_table->port_idx++;
  }

  if (cur == 0)
  {
    yieldlock_release(&(ports_table->lock));
    return -1;
  }

  ports_table->port_idx = PRIV_PORT;
  uplim = cur;
  cur = 0;
  goto repeat;
}



int
ports_reserve_priv(u_short *port, u_int envid)
{
  int r;
  
  dprintf("reserve_priv: %d\n", *port);
  
  if (*port == 0)
  {
    r = ports_get_priv(envid);

    dprintf("get_priv --> %d\n",r);
    if (r < 0)
      RETERR(V_ADDRNOTAVAIL);
    
    *port = r;

    return 0;
  }

  yieldlock_acquire(&(ports_table->lock));

  if (ports_table->ports[*port] == 0 ||
      ports_table->ports[*port] == envid)
  {
    ports_table->ports[*port] = envid;
    yieldlock_release(&(ports_table->lock));
    return 0;
  }
  else 
  {
    int holder = ports_table->ports[*port];
    if (!env_alive(holder))
    {
      ports_table->ports[*port] = envid;
      yieldlock_release(&(ports_table->lock));
      return 0;
    }
  }

  r = ports_table->ports[*port];
  dprintf("%d has it\n",r);
  yieldlock_release(&(ports_table->lock));
  
  return r;
}



int
ports_bind_udp_priv(u_short *port, u_char *ipsrc, int ringid, u_int envid)
{
  int fid;
  int r, newport = 0;
 
  newport = (*port == 0);

  r = ports_reserve_priv(port, envid);
  if (r != 0)
    RETERR(V_ADDRNOTAVAIL);

  if ((fid = dpf_bind_udpfilter(*port, ipsrc, ringid, envid)) < 0)
  {
    if (newport)
      ports_unbind_priv(*port);
  
    RETERR(V_ADDRNOTAVAIL);
  }

  return fid;
}


int
ports_bind_tcp_priv(u_short *localport, int remoteport, int ringid, u_int envid)
{
  int fid;
  int r, newport=0;

  newport = (*localport == 0);
  
  dprintf("bind_tcp_priv: %d %d %d\n", *localport, remoteport, ringid);
  r = ports_reserve_priv(localport, envid);
  dprintf("returned r %d\n",r);
  
  if (r != 0)
    RETERR(V_ADDRNOTAVAIL);

  dprintf("bind_tcp_priv: %d %d %d\n", *localport, remoteport, ringid);

  if ((fid = dpf_bind_tcpfilter(*localport, remoteport, ringid, envid)) < 0)
  {
    if (newport)
      ports_unbind_priv(*localport);

    RETERR(V_ADDRNOTAVAIL);
  }

  return fid;
}




static int ret_arg2_bind_udp;

static int ports_bind_udp_reth(int, int, int, int, u_int) 
  __attribute__ ((regparm (3)));

static int 
ports_bind_udp_reth(int ret1, int ret2, int reqid, int ipctype, u_int caller_id)
{
  ret_arg2_bind_udp = ret2;
  return ret1;
}

int 
ports_bind_udp(u_short *port, u_char *ipsrc, int ringid)
{
  int trusted = ((va2pte((u_int)ports_table) & PG_W) != 0);
 
  dprintf("trusted? %d\n", trusted);
  
  if (port == NULL)
    RETERR(V_INVALID);

  if (trusted)
    return ports_bind_udp_priv(port,ipsrc,ringid,getpid());
  
  else
  {
    int r;
    int arg2;
    
    memmove(&arg2, ipsrc, 4);
    r = ipc_sleepwait(ports_table->ipc_taskid_bind_udp,*port,arg2,1,
	              (u_int)&ringid,ports_bind_udp_reth,
		      ports_table->owner_envid,2);
    if (r < 0)
      RETERR(V_ADDRNOTAVAIL)

    else
    {
      *port = ret_arg2_bind_udp;
      return r;
    }
  }

  assert(0);
  return -1;
}


int
ports_unbind(u_short port)
{
  int trusted = ((va2pte((u_int)ports_table) & PG_W) != 0);
  
  dprintf("trusted? %d\n", trusted);

  if (trusted)
    ports_unbind_priv(port);

  else
    ipc_sleepwait(ports_table->ipc_taskid_unbind,port,0,0,0L,
	          0L,ports_table->owner_envid,0);
  return 0;
}


static int ret_arg2_reserve;
static int ports_reserve_reth(int, int, int, int, u_int) 
  __attribute__ ((regparm (3)));

static int 
ports_reserve_reth(int ret1, int ret2, int reqid, int ipctype, u_int caller_id)
{
  ret_arg2_reserve = ret2;
  return ret1;
}

int
ports_reserve(u_short *port)
{
  int trusted = ((va2pte((u_int)ports_table) & PG_W) != 0);
  
  dprintf("trusted? %d\n", trusted);

  if (port == NULL)
    RETERR(V_INVALID);

  dprintf("trying to reserve %d\n", *port);
  
  if (trusted)
    return ports_reserve_priv(port, getpid());

  else
  {
    int r;
    
    r = ipc_sleepwait(ports_table->ipc_taskid_reserve,*port,0,0,0L,
	              ports_reserve_reth,
		      ports_table->owner_envid,2);
    if (r < 0)
      RETERR(V_ADDRNOTAVAIL)

    else
    {
      *port = ret_arg2_reserve;
      return r;
    }
  }
}



static int ret_arg2_bind_tcp;
static int ports_bind_tcp_reth(int, int, int, int, u_int) 
  __attribute__ ((regparm (3)));

static int 
ports_bind_tcp_reth(int ret1, int ret2, int reqid, int ipctype, u_int caller_id)
{
  ret_arg2_bind_tcp = ret2;
  return ret1;
}

int 
ports_bind_tcp(u_short *localport, int remoteport, int ringid)
{
  int trusted = ((va2pte((u_int)ports_table) & PG_W) != 0);
 
  dprintf("trusted? %d\n", trusted);

  if (localport == NULL)
    RETERR(V_INVALID);

  if (trusted)
    return ports_bind_tcp_priv(localport,remoteport,ringid,getpid());
  
  else
  {
    int r;
    
    r = ipc_sleepwait(ports_table->ipc_taskid_bind_tcp,
	              *localport,remoteport,1,
	              (u_int)&ringid,ports_bind_tcp_reth,
		      ports_table->owner_envid,2);
    if (r < 0)
      RETERR(V_ADDRNOTAVAIL)

    else
    {
      *localport = ret_arg2_bind_tcp;
      return r;
    }
  }

  assert(0);
  return -1;
}


