
#include <stdio.h>
#include <xok/mmu.h>

#include <vos/errno.h>
#include <vos/ipc-services.h>
#include <vos/proc.h>
#include <vos/sbuf.h>
#include <vos/ipc.h>
#include <vos/assert.h>

#include <vos/ipcport.h>


#define dprintf if(0) kprintf

ipcport_t ipcports[NPROC] = {{0,0,0}, };


static int
ipcport_map_h(int task, int p, int k, int ipc_type, u_int envid, void* data)
  __attribute__ ((regparm (3)));
static int
ipcport_map_h(int task, int p, int k, int ipc_type, u_int envid, void* data)
{
  u_int va;
  Pte pte = p;
 
  dprintf("env %d: ipcport_map_h received %d %d\n",getpid(),pte,k);

  if ((pte>>PGSHIFT) == 0)
    RETERR(V_INVALID);

  if ((pte & (PG_U|PG_W|PG_P|PG_SHARED)) != (PG_U|PG_W|PG_P|PG_SHARED))
    RETERR(V_INVALID);

  va = sbuf_map_page(k, &pte);
  
  dprintf("env %d: ipcport_map_h page mapped %d (va 0x%x)\n",getpid(),pte,va);

  if (va < 0)
    RETERR(V_INVALID);
 
  ipcports[envidx(envid)].pte = pte;
  ipcports[envidx(envid)].va = va;
  ipcports[envidx(envid)].k = k;

  dprintf("env %d: ipcport_map_h succeeded\n",getpid());

  return 0;
}


static int ipcport_map_rh(int, int, int, int, u_int)
  __attribute__ ((regparm (3)));
static int
ipcport_map_rh(int ret,int errno,int reqid,int ipc_type,u_int envid)
{
  dprintf("env %d: ipcport_map_rh got %d\n",getpid(),ret);
  return ret;
}


static int
ipcport_map(u_int va, int k, pid_t pid)
{
  Pte pte = va2pte(va);
  
  int r = ipc_sleepwait(IPC_MAP_IPCPORT, pte, k, 0, 0L, 
                        ipcport_map_rh, pid, 1);

  if (r < 0 && errno == V_WOULDBLOCK)
  {
    printf("env %d: pct_map_ipcport timed out\n",getpid());
    RETERR(V_WOULDBLOCK);
  }

  else if (r < 0)
  {
    printf("env %d: pct_map_ipcport failed, errno %d\n",getpid(), errno);
    RETERR(V_CONNREFUSED);
  }

  assert(r==0);

  ipcports[envidx(pid)].pte = pte;
  ipcports[envidx(pid)].k = k;
  ipcports[envidx(pid)].va = va;
  dprintf("env %d: pct_map_ipcport_reth returned %d\n", getpid(), r);
  dprintf("env %d: pct_map_ipcport succeeded\n",getpid());
  return 0;
}


int
ipcport_create(int k, pid_t pid)
{
  int va;
  
  Pte pte = PG_U|PG_W|PG_P|PG_SHARED;
  va = sbuf_map_page(k,&pte);

  if (va < 0)
    return va;

  return ipcport_map(va, k, pid);
}


int
ipcport_name(pid_t pid, char s[])
{
  extern void uitoa(int, char *);
  char t[32];
  strcpy(s, "/dev/ipcport");
  uitoa(pid, t);
  strcat(s, t);
  return 0;
}


void
ipcport_init(void)
{
  int r = ipc_register_handler(IPC_MAP_IPCPORT, ipcport_map_h);
  if (r < 0)
  {
    printf("ipcport_init: cannot register handler for ipcports. "
	   "IPCports will not work\n");
  }
}



