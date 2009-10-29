
#include <stdio.h>
#include <stdlib.h>

#include <xok/mmu.h>
#include <xok/sys_ucall.h>

#include <vos/kprintf.h>
#include <vos/proc.h>

#include <vos/vm-layout.h>
#include <vos/ipc-services.h>
#include <vos/ipc.h>
#include <vos/vm.h>
#include <vos/wk.h>
#include <vos/errno.h>
#include <vos/uinfo.h>
#include <vos/cap.h>

#include <vos/net/arptable.h>
#include <vos/net/iptable.h>
#include "init.h"

char *progname = "init";


static int 
init_pct_handler(int,int,int,int,u_int,void*) __attribute__ ((regparm (3)));

static int
init_pct_handler(int task,int arg1,int arg2,int ipctype,u_int envid,void* d)
{
  if (task == ARPTABLE_ADD_TIMEDOUT)
  {
    extern int arptable_add(u_char *ip, u_char *eth, u_int status);
    u_char *ipaddr = (u_char*) (&arg1); /* arg1: ipaddr */

    arptable_add(ipaddr, 0L, TIMEDOUT);
    return 0;
  }

  else 
    RETERR(V_PROGUNAVAIL);
}


void
__init_start(void)
{
  int r;
  extern int iptable_alloc(void);
  extern int arptable_alloc(void);
#ifdef VOS_DEBUG_LOCK
  extern int dml_alloc(void);
#endif

  StaticAssert(sizeof(uinfo_t) <= NBPG);

  if ((r = vm_alloc_region(UINFO_REGION, NBPG, CAP_USER, PG_U|PG_P|PG_W)) < 0)
  {
    kprintf("init: vos: cannot initialize global tables. "
	    "some vos programs (e.g. network related) may not work!\n");
    exit(-1);
  }
  bzero(uinfo,sizeof(uinfo_t));
  
  iptable_alloc();
  arptable_alloc();
#ifdef VOS_DEBUG_LOCK
  dml_alloc();
#endif

  r = va2ppn(UINFO_REGION);
  sys_uinfo_set(CAP_USER,r);
}


static void
clear_uinfo()
{
  sys_uinfo_set(CAP_USER,0);
}


static int
wait()
{
#define WK_TERM_SZ 	32
#define WK_ARP  	10
  struct wk_term t[WK_TERM_SZ];
  int sz = 0;
  extern int wk_arp(int sz, struct wk_term *t);

  sz = wk_mktag(sz, t, WK_ARP);
  sz = wk_arp(sz, t);
  assert(WK_TERM_SZ >= sz);
  wk_waitfor_pred(t, sz);

  return UAREA.u_pred_tag;
}


int 
main()
{
  u_char arp = 0;

  extern int  arp_init(void);
  extern void arp_check_ring(void);
  extern void stop_arp_replies_filter(void);

  atexit(stop_arp_replies_filter);
  atexit(clear_uinfo);

  if (iptable_remap() < 0)
    printf("%s: warning: cannot remap iptable\n",progname);

  if (arptable_remap() < 0)
    printf("%s: warning: cannot remap arptable\n",progname);
  
  ipc_register_handler(ARPTABLE_ADD_TIMEDOUT,init_pct_handler);
  
  if (arp_init() == 0) 
    arp = 1;
  
  while(1)
  {
    if (arp)
      arp_check_ring();
    wait();
  }
  return 0;
}


