/*
 * Implements the server part of the ARP protocol, 
 * it responds to requests from other hosts for our IP/Ether mapping.
 */
#include <assert.h>
#include <string.h>

#include <exos/net/ae_net.h>
#include <xok/sys_ucall.h>

#include <netinet/in.h>
#include <stdlib.h>		/* for atoi, atexit */
#include <unistd.h>

#include <exos/cap.h>		/* for CAP_ROOT */
#include <exos/uwk.h>
#include <exos/process.h>
#include <exos/critical.h>
#include <exos/ipcdemux.h>
#include <exos/osdecl.h>

#include <xok/sysinfo.h>

#include <signal.h>

#include <exos/netinet/fast_arp.h>
#include <exos/net/if.h>

#include <stdio.h> /* for fflush */

#include "../../lib/libexos/net/arp_table.h"
#define ARINGBUF		/* define if you want to use ring buffers or ashes */

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif /* !MIN */

#define CHECK(status) {\
  if (status < 0) kprintf("WARNING %d at %s %s:%d\n",\
			  status,__FUNCTION__,__FILE__,__LINE__);\
								   }
#ifndef ETHER_MAX_LEN
#define ETHER_MAX_LEN 1500
#endif


#define ARP_CAP 0

typedef struct ringbuf {
  struct ringbuf *next;		/* if next is NULL is not utilized */
  u_int *pollptr;		/* should point to poll */
  u_int flag;
  int n;			/* should be 1 */
  int sz;			/* should be RECVFROM_BUFSIZE */
  char *data;			/* should point to space */
  char space[ETHER_MAX_LEN]; 
} ringbuf_t, *ringbuf_p;

#ifdef ARINGBUF
#define RINGSZ 48
ringbuf_t pkts[RINGSZ];
ringbuf_p pkt;
#else
struct ae_eth_pkt *pkt = 0;
#endif
static int arp_fid_res = 0;
static int ringid = 0;


void check_arp_replies(void);
void stop_arp_replies_filter(void);
static int arp_send_request(unsigned char ip_addr[4], int ifnum);
void arp_insert_reply(struct arprarp *ea);
void arp_respond_request(struct arprarp *ea);
int arp_daemon(void);
void sleep_until_next_event(arp_tick_t next_timeout);

int use_my_own = 1; /* vos compatibility - if 1, then don't use vos arpd stuff */
extern int vos_compat();

	/* Keep this as a global variable!!! */
int start_arp_replies (void);

int pid = 0;
static void arpd_cleanup(void) {
  kprintf("killing arpd\n");
  set_arp_envid(0);
  if (pid) kill(pid,SIGHUP);
}

void arp_check_entry(arp_entry_p e);



int
my_arp_ipc_handler(int code, int ip_addr, int ifnum, int arg3, u_int caller) 
{
  printf("my_arp_ipc_handler: %d, %d\n",code, ip_addr);
  return arp_ipc_handler(code, ip_addr, ifnum, arg3, caller);
}

int 
arpd_main(void) {
  arp_tick_t next_timeout = 0;
  arp_entry_p e;
  int i;

  printf("exos arpd: arp_table at %p\n",arp_table);
  setproctitle("arpd");
  unprotect_arp_table();
  
  // printf("setting arp envid\n");
  set_arp_envid(__envid);
 
  //arp_print_table();
  atexit(arpd_cleanup);
  ipc_register(IPC_ARP_ADD,arp_ipc_handler);
  ipc_register(IPC_ARP_DELETE,arp_ipc_handler);

  if (vos_compat() == 0)
    use_my_own = 0;
  
  printf("exos arpd: arp_table at %p\n",arp_table);
 
  start_arp_replies();

  for(;;) {

    //printf("checking for new entries %d or next_timeout %qd\n",arp_table->new_entry,next_timeout);

    /* check for arp_table_new_entry, new packets, or timeouts */
    if (use_my_own==0 || (arp_table->new_entry == 1 || (next_timeout && TICKS >= next_timeout))) 
    {
      arp_table->new_entry = 0;
      arp_iter_init();
      while((e = arp_iter()))
	arp_check_entry(e);
    }
    //printf("checking replies\n");
    if (use_my_own == 1)
      check_arp_replies();

    next_timeout = 0;
    EnterCritical();
    for (i = 0, e = &arp_table->entries[0] ; i < ARP_TABLE_SIZE ; i++,e++) {
      if (EISFREE(e) || EISTIMEOUT(e) || EISRESOLVED(e)) continue;
      /* we are only goint to act on pending entries that need to retransmit
       * or become timeout */
      assert(EISPENDING(e));
      if (next_timeout == 0) 
	next_timeout = e->ticks;
      else 
	next_timeout = MIN(next_timeout,e->ticks);
    }
    ExitCritical();
    sleep_until_next_event(next_timeout);
  }
}
void
arp_check_entry(arp_entry_p e) {
  arp_tick_t curtick = TICKS;

  EnterCritical();
  if (EISPENDING(e) && curtick > e->ticks) {
    if (use_my_own == 0) 
      /* if in vos table, grab it and update our (exos) table */
    {
      extern int vos_in_table(ipaddr_t ip,ethaddr_t eth);
      unsigned char eth[6];
      if (vos_in_table(e->ip,&eth[0])==1)
      {
        arp_set_resolved(e,e->ip,&eth[0]);
        ExitCritical();
	return;
      }
    }

    if (e->count > 0) {
      arp_retransmit(e);
      ExitCritical();
      arp_send_request(e->ip,e->ifnum);
    } else {
      arp_set_timeout(e);
      ExitCritical();
    }
  } else {
      ExitCritical();
  }
}





/* Send a query for an Ethernet address of a host with IP address */ 
static int 
arp_send_request(unsigned char ip_addr[4], int ifnum) {
  struct arp *arp;
  struct eth *eth;
  struct arprarp *ea;
  static char arp_packet_query[sizeof (struct arprarp)];
  extern int retran;
  extern char eth_bcast_addr[6];
/*
kprintf ("arp_sendreply\n");
*/
  ea = (struct arprarp *)&arp_packet_query;
  arp = &ea->arp;
  eth = &ea->eth;

  memcpy(eth->dst_addr, eth_bcast_addr, sizeof eth->dst_addr);
  get_ifnum_ethernet(ifnum,eth->src_addr );

  eth->proto = htons(EP_ARP);
  arp->arp_hrd = htons(1);
  arp->arp_pro = htons(EP_IP);
  arp->arp_hln = 6;
  arp->arp_pln = 4;
  arp->arp_op = htons(ARP_REQ);

  get_ifnum_ethernet(ifnum,(char *)&arp->arp_sha);
  get_ifnum_ipaddr(ifnum,(char *)&arp->arp_spa);
  memset(&arp->arp_tha, 0, sizeof(arp->arp_sha));
  memcpy(&arp->arp_tpa, ip_addr, sizeof(arp->arp_spa));

  //kprintf("ARP: querying for Ether of IP: %s ",pripaddr(ip_addr));

  while(ae_eth_send(ea, sizeof( struct arprarp), get_ifnum_cardno(ifnum)) < 0) {
    kprintf("arpd: retry arp request after 1 second\n");
    retran++;	/* and retransmit, if necessary */
    sleep(1);
  }

  return 1;
}

static void 
arp_print(struct arprarp *rp)
{
  printf("ARP/RARP OP: %x\nDST: %s",htons(rp->arp.arp_op),
	 prethaddr(rp->eth.dst_addr));
  printf(" SRC: %s",prethaddr(rp->eth.src_addr));
  printf(" proto: %04x\n",htons(rp->eth.proto));
  printf("arp: hrd %d pro 0x%x hlen %d plen %d op: %d\n", 
	 rp->arp.arp_hrd, rp->arp.arp_pro, rp->arp.arp_hln,
	 rp->arp.arp_pln, htons(rp->arp.arp_op));
  printf("src h %s\n",prethaddr(rp->arp.arp_sha));
  printf("src p %s\n",pripaddr(rp->arp.arp_spa)); 
  printf("dst h %s\n",prethaddr(rp->arp.arp_tha));
  printf("dst p %s\n",pripaddr(rp->arp.arp_tpa)); 
}

int 
start_arp_replies (void)
{
    void *filter;
    int sz;
#ifdef ARINGBUF
    int i;
    /* pkt statically  */
    for(i = 0; i < RINGSZ; i++) {
      pkt = &pkts[i];
      pkt->next = &pkts[i+1];
      pkt->pollptr = &pkts[i].flag;
      pkt->n = 1;
      pkt->sz = ETHER_MAX_LEN;
      pkt->data = &pkt->space[0];
      pkt->flag = 0;
    }
    /* fix last entry */
    pkt->next = &pkts[0];
#else 
    pkt = ae_eth_get_pkt();
    if (pkt == (struct ae_eth_pkt *)0) {
      printf("could not allocate receive buffer\n");
      assert (0);
    }
    pkt->flag = -1;
#endif

    filter = mk_arp_getall(&sz); 
    // kprintf("inserting arp replies filter...");
    fflush(stdout);
#ifdef ARINGBUF
    if ((ringid = sys_pktring_setring (ARP_CAP, 0, (struct pktringent *) pkt)) <= 0) {
      kprintf ("setring failed: no soup for you, returned: %d!\n",ringid);
      assert (0);
    }
#endif
    if(((int)(arp_fid_res = sys_self_dpf_insert_old(CAP_ROOT, ARP_CAP, filter, sz, ringid))) < 0) {
      printf("exos arpd: failed to insert ARP filter, _assuming_ someone is already listening\n");
      if (use_my_own == 1)
        exit (0);
    }
    else
      kprintf("exos arpd: arp replies filter: using filter %d, pkt: %p, env: %d\n",arp_fid_res,pkt,__envid);
#ifdef ARINGBUF
    pkt->flag = 0;
#else
    pkt->flag = -1;
    ae_eth_poll(arp_fid_res, pkt); 
#endif

    if((atexit(stop_arp_replies_filter)) < 0) {
      printf("warning OnExit returned less than 0\n");
    }

#ifdef ARINGBUF
    pkt->flag = 0;
#else
    pkt->flag = -1;
#endif

#ifdef ARINGBUF
    //kprintf("(using pktrings, ringid: %d)\n",ringid);
#else
    //kprintf("(using ashes)\n");
#endif
    return (1);
}

void 
stop_arp_replies_filter(void) {
  int status;
  demand(arp_fid_res > 0, trying to delete invalid fid);
  kprintf("stop_arp_requests_filter: fid:%d ring_id: %d\n",arp_fid_res,ringid);
  printf("deleting fid %d\n",arp_fid_res);

  status = sys_self_dpf_delete(ARP_CAP, arp_fid_res);
  CHECK(status);
#ifdef ARINGBUF
  status = sys_pktring_delring(ARP_CAP, ringid);
  CHECK(status);
#else
  ae_eth_close(arp_fid_res);
  ae_eth_release_pkt(pkt);
#endif
}

static int 
wk_mkneq_pred(int sz, struct wk_term *t, int *addr, int value, u_int cap) {
  sz = wk_mkvar (sz, t, wk_phys (addr), cap);
  sz = wk_mkimm (sz, t, value);
  sz = wk_mkop (sz, t, WK_NEQ);
  return (sz);
}

int
wk_mkpkt_pred(int sz, struct wk_term *t) {
#ifdef ARINGBUF
  return wk_mkneq_pred (sz, t,&pkt->flag, 0, CAP_ROOT);
#else
  return wk_mkneq_pred (sz, t,&pkt->flag, 0xFFFFFFFF, CAP_ROOT);
#endif
}

void sleep_until_next_event(arp_tick_t next_timeout) {
#define WK_TERM_SZ (12 + UWK_MKSLEEP_PRED_SIZE)
  struct wk_term t[WK_TERM_SZ];
  int sz = 0;

  if (next_timeout != 0) {
    /* wait until next timeout */
    sz = wk_mksleep_pred(t,next_timeout);
    sz = wk_mkop (sz, t, WK_OR);
  }
  /* wait for a new entry to be added to table */
  sz = wk_mkneq_pred(sz, t, &arp_table->new_entry, 0, CAP_ROOT);
  sz = wk_mkop (sz, t, WK_OR);
  /* wait for a network packet */
  sz = wk_mkpkt_pred(sz, t);

  assert(WK_TERM_SZ >= sz);
  
  wk_waitfor_pred(t, sz);
}


void 
check_arp_replies(void) {
  struct arprarp *ea;
  struct arp *arp;
  int cnt = 0;

//kprintf ("check_arp_replies: pkt %p, pkt->flag %d\n", pkt, pkt->flag);
  while (pkt->flag > 0) {
    cnt++;
#ifdef ARINGBUF
    ea = (struct arprarp *)pkt->data;
    arp = &ea->arp;
    if (arp->arp_op == htons(ARP_REP)) {
      //      kprintf("1) REPLY ARRIVED\n");
      arp_insert_reply(ea);
    } else if (arp->arp_op == htons(ARP_REQ)) {
      //      kprintf("2) REQUEST ARRIVED\n");
      arp_respond_request(ea);
    } else {
      //      kprintf("3) UNKNOWN ARP REQUEST ARRIVED\n");
      arp_print(ea);
    }
    
    pkt->flag = 0;
    pkt = pkt->next;
#else
    ea = (struct arprarp *)pkt->recv.r[0].data;
    arp = &ea->arp;

    if (arp->arp_op == htons(ARP_REP)) 
      arp_insert_reply(ea);
    else if (arp->arp_op == htons(ARP_REQ)) 
      print_arp(ea);

    pkt->flag = -1;
    ae_eth_poll(arp_fid_res, pkt); 
#endif
  }
//kprintf ("handled %d ARP-related packets\n", cnt);
}

void 
arp_insert_reply(struct arprarp *ea) {
  arp_entry_p e;
  // kprintf("arp_insert_reply\n");
  // arp_print(ea);
  EnterCritical();
  if ((e = arp_search_entry_by_ip(ea->arp.arp_spa)) != NULL) {
    arp_set_resolved(e,ea->arp.arp_spa,ea->arp.arp_sha);
  }
  ExitCritical();
  
}

/* 
 * respond to requests
 */

void 
arp_respond_request(struct arprarp *ea) {
  struct arp *reply_arp,*arp;
  struct eth *reply_eth,*eth;
  int cardno;
  int ifnum;
  static struct arprarp reply_ea;
  extern int retran;
    
  reply_arp = &reply_ea.arp;
  reply_eth = &reply_ea.eth;
   
  arp = &ea->arp;
  eth = &ea->eth;


  EnterCritical();
  init_iter_ifnum();
  while((ifnum = iter_ifnum(IF_UP)) != -1) {
    get_ifnum_ipaddr(ifnum,(char *)&reply_arp->arp_spa); /* source ip addr */
    /* if their target matches ours */
    if (bcmp(&reply_arp->arp_spa, &arp->arp_tpa, 4) == 0) {
      //kprintf("REQUEST IS FOR OUR MACHINE IP: %s\n",pripaddr((char *)&arp->arp_tpa));
      //arp_print(ea);
      /* is one of my interfaces */
      cardno = get_ifnum_cardno(ifnum);
      get_ifnum_ethernet(ifnum,(char *)&reply_arp->arp_sha);/* source eth addr */
      ExitCritical();
      
      /* set ethernet fields */
      memcpy(reply_eth->src_addr, &reply_arp->arp_sha, 6);
      memcpy(reply_eth->dst_addr, &arp->arp_sha, 6);
      reply_eth->proto = htons(EP_ARP);

      /* copy their source, as our target */
      memcpy(reply_arp->arp_tha, &arp->arp_sha, 6);
      memcpy(reply_arp->arp_tpa, &arp->arp_spa, 4);

      reply_arp->arp_hrd = htons(1);
      reply_arp->arp_pro = htons(EP_IP);
      reply_arp->arp_hln = 6;
      reply_arp->arp_pln = 4;
      reply_arp->arp_op = htons(ARP_REP);

      //arp_print(&reply_ea);

      while(ae_eth_send(&reply_ea, 
			sizeof( struct arprarp), 
			cardno) < 0)
	retran++;	/* and retransmit, if necessary */
      return;
    }

  }
  ExitCritical();
  
}
