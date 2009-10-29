
/*
 * Implements the receiving side of ARP protocol: collects all arp replies and
 * respond to arp queries... Processes should send arp request themselves.
 */

#include <xok/sys_ucall.h>
#include <dpf/dpf.h>
#include <dpf/dpf-internal.h>

#include <vos/net/fast_eth.h>
#include <vos/net/fast_arp.h>

#include <vos/cap.h>
#include <vos/wk.h>
#include <vos/proc.h>
#include <vos/kprintf.h>
#include <vos/assert.h>

#include <vos/net/iptable.h>
#include <vos/net/arptable.h>
#include <vos/net/ae_ether.h>


#define dprintf if (0) kprintf
#define dprint_arpe if (0) print_arpe
#define dprint_ethaddr if (0) print_ethaddr
#define dprint_ipaddr if (0) print_ipaddr


typedef struct ringbuf 
{
  struct ringbuf *next;		/* if next is NULL is not utilized */
  u_int *pollptr;		/* should point to poll */
  u_int flag;
  int n;			/* should be 1 */
  int sz;			/* should be RECVFROM_BUFSIZE */
  char *data;			/* should point to space */
  char space[ETHER_MAX_LEN]; 
} ringbuf_t;

#define RINGSZ 48
static ringbuf_t arp_pkts[RINGSZ];
static ringbuf_t* arp_pkt;


static int arp_fid_res = 0;
static int arp_ringid = 0;
 

static void 
arp_print(struct arprarp *rp)
{
  dprintf("ARP/RARP OP: %x\nDST: ",htons(rp->arp.arp_op));
  dprint_ethaddr((u_char *)(rp->eth.dst_addr));
  
  dprintf(" SRC: "); 
  dprint_ethaddr((u_char *)(rp->eth.src_addr));
  
  dprintf(" proto: %04x\n",htons(rp->eth.proto));
  dprintf("arp: hrd %d pro 0x%x hlen %d plen %d op: %d\n", 
	 rp->arp.arp_hrd, rp->arp.arp_pro, rp->arp.arp_hln,
	 rp->arp.arp_pln, htons(rp->arp.arp_op));
  
  dprintf("src h "); 
  dprint_ethaddr((u_char*)(rp->arp.arp_sha)); 
  dprintf("\n");
  dprintf("src p "); 
  dprint_ipaddr((u_char*)(rp->arp.arp_spa)); 
  dprintf("\n"); 
  dprintf("dst h "); 
  dprint_ethaddr((u_char*)(rp->arp.arp_tha)); 
  dprintf("\n");
  dprintf("dst p "); 
  dprint_ipaddr((u_char*)(rp->arp.arp_tpa)); 
  dprintf("\n"); 
}


int 
arp_init(void)
{
  struct dpf_ir filter;
  int i;

  /* pkt statically  */
  for(i = 0; i < RINGSZ; i++) {
    arp_pkt = &arp_pkts[i];
    arp_pkt->next = &arp_pkts[i+1];
    arp_pkt->pollptr = &arp_pkts[i].flag;
    arp_pkt->n = 1;
    arp_pkt->sz = ETHER_MAX_LEN;
    arp_pkt->data = &arp_pkt->space[0];
    arp_pkt->flag = 0;
  }
  /* fix last entry */
  arp_pkt->next = &arp_pkts[0];

  if ((arp_ringid = sys_pktring_setring
	(CAP_ARP,0,(struct pktringent *) arp_pkt))<=0) 
  {
    kprintf ("vos arpd: setring failed: returned: %d!\n",arp_ringid);
    assert (0);
  }
  
  dpf_begin(&filter);  /* must be done before use */
  /* eth protocol = arp */
  dpf_eq16(&filter, eth_offset(proto), htons(EP_ARP));  
  /* hardware proto = 1 */
  dpf_eq16(&filter, eth_sz+arp_offset(arp_hrd), htons(1));  
  /* target proto = EP_IP */
  dpf_eq16(&filter, eth_sz+arp_offset(arp_pro), htons(EP_IP));  


  dprintf("vos arpd: inserting arp replies filter...\n");
  arp_fid_res = sys_self_dpf_insert(CAP_USER, CAP_ARP, &filter, arp_ringid);
    
  if((int)(arp_fid_res) < 0)
  {
    kprintf("vos arpd: failed to insert ARP filter, already listening?\n");
    return -1;
  }

  kprintf("vos arpd: arp replies filter: using filter %d, pkt: %p, env: %d)\n",
      arp_fid_res,arp_pkt,getpid());
  dprintf("(using pktrings, arp_ringid: %d)\n",arp_ringid);
  arp_pkt->flag = 0;

  return 0;
}


void 
stop_arp_replies_filter(void) 
{
  int status;
  
  if(arp_fid_res <= 0)
  {
    printf("trying to delete invalid fid\n");
    return;
  }

  printf("stop_arp_requests_filter: fid:%d ring_id: %d\n",arp_fid_res,arp_ringid);
  printf("deleting fid %d\n",arp_fid_res);

  status = sys_self_dpf_delete(CAP_ARP, arp_fid_res);
  if (status < 0)
    printf("sys_self_dpf_delete failed\n");

  status = sys_pktring_delring(CAP_ARP, arp_ringid);
  if (status < 0)
    printf("sys_pktring_delring failed\n");
}


void 
arp_insert_reply(struct arprarp *ea) 
{
  extern int arptable_add(u_char *ip, u_char *eth, u_int status);
  
  dprintf("arp_insert_reply\n");
  arp_print(ea);

  arptable_add(ea->arp.arp_spa,ea->arp.arp_sha,RESOLVED);
}


void 
arp_respond_request(struct arprarp *ea) 
{
  struct arp *reply_arp,*arp;
  struct eth *reply_eth,*eth;
  int cardno;
  int ifnum;
  int iter;
  int retran = 0;
  static struct arprarp reply_ea;
    
  reply_arp = &reply_ea.arp;
  reply_eth = &reply_ea.eth;
   
  arp = &ea->arp;
  eth = &ea->eth;

  iter = 0;

  while((ifnum = iptable_find_if(IF_UP, &iter)) != -1) 
  {
    /* source ip addr */
    if_get_ipaddr(ifnum,(char *)&reply_arp->arp_spa); 

    /* if their target matches ours */
    if (bcmp(&reply_arp->arp_spa, &arp->arp_tpa, 4) == 0) 
    {
      dprintf("REQUEST IS FOR OUR MACHINE IP: ");
      dprint_ipaddr((char *)&arp->arp_tpa);
      dprintf("\n");

      // arp_print(ea);

      /* is one of my interfaces */
      cardno = if_get_cardno(ifnum);
      if_get_ethernet(ifnum,(char *)&reply_arp->arp_sha);

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

      // arp_print(&reply_ea);

      while(ae_eth_send(&reply_ea, sizeof(struct arprarp), cardno) < 0)
      {
	retran++;
	if (retran > ARP_RETRANS)
	{
	  printf("arp reply: retransmitted %d times, skip.\n",ARP_RETRANS);
	  break;
	}
      }
      return;
    }
  }
}


void 
arp_check_ring(void) 
{
  struct arprarp *ea;
  struct arp *arp;
  int cnt = 0;

  dprintf ("check_arp_replies: pkt %p, pkt->flag %d\n", arp_pkt, arp_pkt->flag);
  while (arp_pkt->flag > 0) 
  {
    cnt++;
    ea = (struct arprarp *)arp_pkt->data;
    arp = &ea->arp;
    if (arp->arp_op == htons(ARP_REP)) 
    {
      dprintf("1) REPLY ARRIVED\n");
      // arp_print(ea);
      arp_insert_reply(ea);
    } 
    
    else if (arp->arp_op == htons(ARP_REQ)) 
    {
      dprintf("2) REQUEST ARRIVED\n");
      // arp_print(ea);
      arp_respond_request(ea);
    } 
    
    else 
    {
      printf("arpd: warning: unknown arp request arrived\n");
      // arp_print(ea);
    }
    
    arp_pkt->flag = 0;
    arp_pkt = arp_pkt->next;
  }
  dprintf ("handled %d ARP-related packets\n", cnt);
}



static inline int 
wk_mkneq_pred(int sz, struct wk_term *t, int *addr, int value, u_int cap) 
{
  sz = wk_mkvar (sz, t, wk_phys (addr), cap);
  sz = wk_mkimm (sz, t, value);
  sz = wk_mkop (sz, t, WK_NEQ);
  return (sz);
}


static inline int
wk_mkpkt_pred(int sz, struct wk_term *t) 
{
  return wk_mkneq_pred (sz, t,&arp_pkt->flag, 0, CAP_USER);
}


int 
wk_arp(int sz, struct wk_term *t)
{
  /* wait for a network packet */
  return wk_mkpkt_pred(sz, t);
}



