
#include <stdio.h>
#include <stdlib.h>

#include <xok/sys_ucall.h>
#include <dpf/dpf.h>
#include <dpf/dpf-internal.h>

#include <vos/net/fast_eth.h>
#include <vos/net/fast_ip.h>
#include <vos/net/fast_icmp.h>

#include <vos/net/iptable.h>
#include <vos/net/ae_ether.h>
#include <vos/net/cksum.h>

#include <vos/cap.h>
#include <vos/wk.h>


typedef struct ringbuf 
{
  struct ringbuf *next;		
  u_int *pollptr;		/* should point to flag */
  u_int flag;			/* set by kernel to say we have a packet */
  int n;			/* should be 1 */
  int sz;			/* amount of buffer space */
  char *data;			/* should point to space */
  char space[ETHER_MAX_LEN];	/* actual storage space for packet */
} ringbuf_t, *ringbuf_p;

#define RINGSZ 1024		/* how many packets we can buffer at once */
static ringbuf_t icmp_pkts[RINGSZ];	/* allocate space for incoming packets */
static ringbuf_p icmp_pkt;

static int ringid;			/* packet ring id */

#define MAXFILTS 16
static int demuxid[MAXFILTS];		/* array of filters we're using */
static int last_demuxid = 0;		/* index of last filter in demuxid */


#define dprintf if (0) kprintf
#define dprint_ipaddr if (0) print_ipaddr


static int 
setup_packet_rings () 
{
  int i;

  /* setup all the pointers in the ring */
  for(i = 0; i < RINGSZ; i++) 
  {
    icmp_pkt = &icmp_pkts[i];
    icmp_pkt->next = &icmp_pkts[i+1];
    icmp_pkt->pollptr = &icmp_pkts[i].flag;
    icmp_pkt->n = 1;
    icmp_pkt->sz = ETHER_MAX_LEN;
    icmp_pkt->data = &icmp_pkt->space[0];
    icmp_pkt->flag = 0;
  }
  /* fix last entry */
  icmp_pkt->next = &icmp_pkts[0];

  if ((ringid = sys_pktring_setring (CAP_USER, 0, (struct pktringent *) icmp_pkt))
      <= 0) 
    return -1;
  return 0;
}


static int 
setup_packet_filter () 
{
  int ifnum;
  int iter;
  unsigned int ip;

  /* we want a bunch of different types of packets. 
   *
   * First, we want any packet with ip protocol of ICMP
   *  
   * Second, we want any ip or tcp packet not picked up by any other filter.
   * This is so we can send "protocol unreachable" and "port unreachable"
   * messages back. DPF gives packets to the longest matching filter so when a
   * particular program binds a more specific filter than ours they will get
   * the proper packets instead of us. It turns out that to do this, we just
   * grab all ip packets sent to us. In the presence of multiple interfaces,
   * we might have multiple ip addresses, so we insert one filter per ip
   * address, all pointing to the same packet ring 
   */

  iter = 0;

  while ((ifnum = iptable_find_if(IF_UP, &iter)) != -1)
  {
    struct dpf_ir ir;

    /* source ip addr */
    if_get_ipaddr(ifnum, (char *)&ip);

    dpf_begin (&ir);     
    /* eth proto = ip */
    dpf_eq16 (&ir, eth_offset(proto), htons(EP_IP));
    /* ip dest = us */
    dpf_eq32 (&ir, eth_sz+ip_offset(destination), ip);

    demuxid[last_demuxid] = sys_self_dpf_insert(CAP_USER,CAP_USER,&ir,ringid);
    if (demuxid[last_demuxid] < 0) 
      return -1;
    
    printf("setting up ip (icmp) packet filter for interface %d, filter id %d\n",
	ifnum,demuxid[last_demuxid]);
    last_demuxid++;
    
  } /* while */
  return 0;
}


static int 
icmp_init () 
{
  if (setup_packet_rings () < 0)
    return -1;
  if (setup_packet_filter () < 0)
    return -1;
  return 0;
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
  return wk_mkneq_pred (sz, t,&icmp_pkt->flag, 0, CAP_USER);
}


static inline int 
wk_icmp(int sz, struct wk_term *t)
{
  /* wait for a network packet */
  return wk_mkpkt_pred(sz, t);
}


void stop_icmp_filters () 
{
  /* remove the filter */
  while (--last_demuxid >= 0) 
  {
    if (sys_self_dpf_delete (CAP_USER, demuxid[last_demuxid]) < 0) 
      printf("icmpd: could not remove filter on shutdown\n");
  }
  
  /* remove the packet ring */
  if (sys_pktring_delring (CAP_USER, ringid) < 0) 
    printf("icmpd: could not delete packet ring on shutdown\n");
}


/* say we're done with the current packet */
static inline void 
release_packet () 
{
  /* give the current packet back to the kernel */
  icmp_pkt->flag = 0;

  /* and get ready to process the next packet */
  icmp_pkt = icmp_pkt->next;
}

/* prepare dst to be sent in respone to src. That is, copy the ethernet
   and ip addresses and setup dst's ip header (except total length
   and checksum). returns a pointer to dst's ip header. */

static inline struct ip *
setup_packet (char *dst, char *src) 
{
  struct eth *dst_eth, *src_eth;
  struct ip *dst_ip, *src_ip;

  /* setup some usefull pointers */
  dst_eth = (struct eth *)dst;
  src_eth = (struct eth *)src;
  dst_ip = (struct ip *)((char *)dst_eth + eth_sz);
  src_ip = (struct ip *)((char *)src_eth + eth_sz);

  /* fill in destination ethernet address */
  /* don't have to fill in the dst eth addr since our caller will lookup
     the proper routing info and do that for us */
  memcpy (dst_eth->src_addr, src_eth->dst_addr, sizeof (src_eth->src_addr));
  dst_eth->proto = htons (EP_IP);
  
  /* fill in the destination ip address */
  dst_ip->vrsihl = (0x4 << 4) | (sizeof(struct ip) >> 2);  
  dst_ip->tos = 0;
  dst_ip->identification = 1;
  dst_ip->fragoffset = 0;
  dst_ip->ttl = 0xff;
  dst_ip->protocol = IP_PROTO_ICMP;
  dst_ip->source[0] = src_ip->destination[0];
  dst_ip->source[1] = src_ip->destination[1];
  dst_ip->destination[0] = src_ip->source[0];
  dst_ip->destination[1] = src_ip->source[1];

  return ((struct ip *)dst_ip);
}

/* pass in pointer to ip header and length of user data beyond the
   ip header and update the ip header */

static inline void 
finish_packet (struct ip *ip, int icmp_len) 
{
  ip->totallength = htons (icmp_len + ip_sz);
  ip->checksum = 0;
  ip->checksum = inet_checksum((uint16 *) ip, sizeof(struct ip), 0, 1);
}

static inline void 
send_packet(char *data, int len, int ifnum, ethaddr_t dst) 
{
  struct eth *eth;
  int retrans;
  int cardno = if_get_cardno(ifnum);

  eth = (struct eth *)data;
  memcpy (eth->dst_addr, dst, sizeof (ethaddr_t));

  for (retrans = 0; retrans < 20; retrans++) 
  {
    if (ae_eth_send (data, len, cardno) >= 0) 
      break;
  }
  if (retrans == ICMP_RETRANS) 
  {
    printf("icmpd: time-out sending packet after %d tries.\n", ICMP_RETRANS);
  }
}


/* build a reply in packet to the echo request in pkt->data */
static inline int 
send_echo_reply_packet (char *packet) 
{
  struct ip *src_ip, *dst_ip;
  struct icmp_echo *icmp;
  struct eth *src_eth, *dst_eth;

  src_ip = (struct ip *)(icmp_pkt->data+eth_sz);
  dst_ip = (struct ip *)(packet+eth_sz);
  dst_eth = (struct eth *)(packet);
  src_eth = (struct eth *)(icmp_pkt->data);
  icmp = (struct icmp_echo *)(packet+eth_sz+ip_sz);
  
  /* drop any fragmented packets */
  if (ntohs (src_ip->fragoffset)) 
  {
    printf("icmpd: dropping fragmented echo request\n");
    return 0;
  }

  /* an echo request is just a copy of the incoming data with a few
     fields changed */

  memcpy (packet, icmp_pkt->data, ntohs (src_ip->totallength)+eth_sz);
  memcpy (dst_eth->src_addr, src_eth->dst_addr, sizeof (src_eth->dst_addr));
  memcpy (dst_ip->destination, src_ip->source, sizeof (src_ip->source));
  memcpy (dst_ip->source, src_ip->destination, sizeof (src_ip->destination));
  icmp->type = ICMP_ECHO_REPLY;
  icmp->checksum = 0;
  icmp->checksum = inet_checksum((uint16 *)icmp,sizeof(struct icmp_echo),0,1);
  return (ntohs (src_ip->totallength)+eth_sz);
}


static inline int 
process_icmp_packet (char *packet) 
{
  struct icmp_generic *icmp_in;
  int len;

  icmp_in = (struct icmp_generic *)(icmp_pkt->data+eth_sz+ip_sz);
  
  switch (icmp_in->type) 
  {
    case ICMP_ECHO: 
      len = send_echo_reply_packet (packet);
      break;
    
    default: 
      // printf("icmpd: bogus ICMP packet. dropping it.\n"); 
      len = 0;
      break;
  }

  return len;
}


static inline int 
process_port_unreachable (char *packet) 
{
  struct icmp_unreach *icmp;
  struct ip *dst_ip, *src_ip;

  /* pass in new packet and incoming packet and setup the 
     new packet's eth and ip headers */

  dst_ip = setup_packet (packet, icmp_pkt->data);

  /* setup some other usefull pointers */
  icmp = (struct icmp_unreach *)((char *)dst_ip + ip_sz);
  src_ip = (struct ip *)((char *)icmp_pkt->data+eth_sz);
  
  /* and actually fill in this packet */

  icmp->type = ICMP_DST_UNREACHABLE;
  icmp->code = ICMP_CODE_PORTUNREACH;
  icmp->unused = 0;
  icmp->ip = *src_ip;

  /* copy first part of incoming packet back out */
  memcpy (icmp->data, icmp_pkt->data+eth_sz+ip_sz, sizeof (icmp->data));

  icmp->checksum = 0;
  icmp->checksum = inet_checksum ((uint16 *)icmp, sizeof (struct icmp_unreach),
				  0, 1);

  /* set ip length and compute ip checksum */
  finish_packet (dst_ip, sizeof (struct icmp_unreach));
  return (sizeof (struct icmp_unreach) + eth_sz + ip_sz);
}


static inline int 
process_protocol_unreachable (char *packet) 
{
  printf ("got protocol unreachable packet\n");
  return 0; 
}


static inline void 
icmp_check_ring(void)
{
  struct ip *ip;
  char packet[256];   /* better be bigger than max icmp packet */
  int len;
  int cnt = 0;
  int ifnum = 0;
  ethaddr_t dsteth;

  while (icmp_pkt->flag > 0) 
  {
    cnt++;

    /* cast the packet data to an ip header struct */
    ip = (struct ip *)((char *)icmp_pkt->data + eth_sz);

    dprintf("icmp for: "); 
    dprint_ipaddr((unsigned char*)ip->destination); 
    dprintf("\n");

    /* lookup how to route our response back to the source */
    if (ipaddr_get_dsteth ((unsigned char *)ip->source, dsteth, &ifnum) < 0)
    {
      dprintf("icmp: don't know how to send icmp response, skip.\n");
      release_packet ();
      continue;
    }

    if ((ifnum = iptable_find_if_ipaddr((unsigned char *)(ip->destination)))<0)
    {
      dprintf("icmp: apparently not for us, skip.\n");
      release_packet ();
      continue;
    }

    /* and demux what type of packet this is */
    switch (ip->protocol) 
    {
      case IP_PROTO_ICMP: 
	len = process_icmp_packet(packet); 
	break;
      case IP_PROTO_TCP:
      case IP_PROTO_UDP: 
	// kprintf("process port unreachable, %d\n", ifnum);
	len = process_port_unreachable(packet); 
	break;
      default: 
	len = process_protocol_unreachable(packet); 
	break;
    }
  
  
    /* send the outgoing packet if there is one */
    if (len) 
      send_packet (packet, len, ifnum, dsteth);

#if 0
    if (ip->protocol == IP_PROTO_TCP ||
	ip->protocol == IP_PROTO_UDP) 
      kprintf("release packet\n");
#endif
    release_packet ();
  }
  dprintf ("handled %d ICMP-related packets\n", cnt);
}


static inline int
wait()
{
#define WK_TERM_SZ 	32
#define WK_ICMP 	11
  struct wk_term t[WK_TERM_SZ];
  int sz = 0;
  extern int wk_icmp(int sz, struct wk_term *t);

  sz = wk_mktag(sz, t, WK_ICMP);
  sz = wk_icmp(sz, t);
  assert(WK_TERM_SZ >= sz);
  wk_waitfor_pred(t, sz);
  return UAREA.u_pred_tag;
}


int 
main()
{
  atexit(stop_icmp_filters);

  if (icmp_init() != 0) 
  {
    printf("icmp: initialization failed.\n");
    exit(-1);
  }

  while(1)
  {
    icmp_check_ring();
    wait();
  }
  return 0;
}


