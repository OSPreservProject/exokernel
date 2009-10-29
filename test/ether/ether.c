
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <netdb.h>
#include <xok/sysinfo.h>
#include <xok/sys_ucall.h>

#include <exos/netinet/hosttable.h>
#include <exos/osdecl.h>

#include <xio/xio_net_wrap.h>
static xio_nwinfo_t nwinfo;

#define DIRECT_XMIT

static struct ae_recv outpkt_space;
static struct ae_recv *outpkt = &outpkt_space;
static char ctlpacket[ETHER_MAX_LEN];
static char datapacket[ETHER_MAX_LEN];

static int isserver;
static char *host;
static char *myhost;
static uint16 dstportno = 3745;
static uint16 srcportno = 3745;
static uint32 ip_dstaddr;
static uint32 ip_myaddr;
static char eth_dstaddr[6];
static char eth_myaddr[6];
static int netcardno = -1;

static int startinpackets = 0;
static int startoutpackets = 0;

static int pktcnt = 0;
static int pktdata = 0;
static int ackfreq = 0;
static int ackdata = 0;

#include <xok_include/net/ip.h>
#include <xok_include/net/ether.h>

#define PKT_START	1
#define PKT_STARTACK	2
#define PKT_STOP	3
#define PKT_STOPACK	4
#define PKT_DATA	5
#define PKT_DATAACK	6

struct pkt {
   char ether[sizeof(struct ether_pkt)];
   char ip[sizeof(struct ip_pkt)];
   char udp[sizeof(struct udp_pkt)];
   int type;
   int id;
   unsigned char data[4];
};

struct start_pkt {
   char ether[sizeof(struct ether_pkt)];
   char ip[sizeof(struct ip_pkt)];
   char udp[sizeof(struct udp_pkt)];
   int type;
   int id;
   int pktcnt;
   int pktdata;
   int ackfreq;
   int ackdata;
};


/*
 * Simple program to measure minimum (driver-driver) roundtrip times.
 * The interesting stuff is in the kernel.
 */

static void
eth_print(char *a)
{
    int i;

    for (i = 0; i < 6; i++) {
	printf("%x:", a[i] & 0xFF);
    }
    printf("\n");
}


static void
ip_print(char *a)
{
    int i;

    for (i = 0; i < 4; i++) {
	printf("%d.", a[i] & 0xFF);
    }
    printf("\n");
}

static void
init(void)
{
    struct hostent *addr;
    char *eth_addr;

    if ((addr=gethostbyname(host)) == NULL) {
	perror("bad hostname");
	exit(1);
    }
    memcpy((char*)&ip_dstaddr, addr->h_addr, addr->h_length);

    if ((addr=gethostbyname(myhost)) == NULL) {
	perror("bad local hostname");
	exit(1);
    }
    memcpy((char*)&ip_myaddr, addr->h_addr, addr->h_length);

    eth_addr = get_ether_from_ip((char *)&ip_dstaddr, 1);
    memcpy(&eth_dstaddr, eth_addr, 6);

    eth_addr = get_ether_from_ip((char *)&ip_myaddr, 1);
    memcpy(&eth_myaddr, eth_addr, 6);

    printf("me: %x ", ip_myaddr);
    ip_print((char *)&ip_myaddr);
    eth_print((char *)&eth_myaddr);

    printf("dst: %x ", ip_dstaddr);
    ip_print((char *)&ip_dstaddr);
    eth_print((char *)&eth_dstaddr);

    fflush(stdout);
}


void setnetcardno (char * eth_addr)
{
   int i;

   for (i=0; i<__sysinfo.si_nnetworks; i++) {
      if (bcmp (eth_addr, __sysinfo.si_networks[i].ether_addr, 6) == 0) {
         if (netcardno != i) {
            startinpackets = __sysinfo.si_networks[i].rcvs;
            startoutpackets = __sysinfo.si_networks[i].xmits;
            netcardno = i;
         }
         return;
      }
   }
   assert (0);
}


void init_packet (struct pkt *pkt)
{
   bzero ((char *)pkt, sizeof(struct pkt));

   memcpy(((struct ether_pkt *)pkt->ether)->ether_dhost, &eth_dstaddr, 6);
   memcpy(((struct ether_pkt *)pkt->ether)->ether_shost, &eth_myaddr, 6);
   ((struct ether_pkt *)pkt->ether)->ether_type = htons(ETHERTYPE_IP);

   ((struct ip_pkt *)pkt->ip)->ip_hl = sizeof(struct ip_pkt) >> 2;
   ((struct ip_pkt *)pkt->ip)->ip_v = 0x4;
   ((struct ip_pkt *)pkt->ip)->ip_ttl = 0xFF;
   ((struct ip_pkt *)pkt->ip)->ip_p = IPPROTO_UDP;
   ((struct ip_pkt *)pkt->ip)->ip_dst = ip_dstaddr;
   ((struct ip_pkt *)pkt->ip)->ip_src = ip_myaddr;
   ((struct ip_pkt *)pkt->ip)->ip_len = htons(sizeof(struct pkt) - sizeof(pkt->ether));

   ((struct udp_pkt *)pkt->udp)->udp_dport = htons(dstportno);
   ((struct udp_pkt *)pkt->udp)->udp_sport = htons(srcportno);
printf ("dstportno %d, srcportno %d\n", ((struct udp_pkt *)pkt->udp)->udp_dport, ((struct udp_pkt *)pkt->udp)->udp_sport);
   ((struct udp_pkt *)pkt->udp)->udp_len = htons(sizeof(struct pkt) - sizeof(pkt->ether) - sizeof(pkt->ip));

   pkt->type = PKT_START;
}


void init_packets ()
{
   init_packet ((struct pkt *) datapacket);
   init_packet ((struct pkt *) ctlpacket);
}


void setup_packet (struct ae_recv *outpkt, int type, int datalen)
{
   struct pkt *pkt = (struct pkt *) datapacket;
   outpkt->n = 1;
   outpkt->r[0].sz = max (ETHER_MIN_LEN, (sizeof(struct pkt) + datalen - 4));
   outpkt->r[0].data = datapacket;
   pkt->type = type;
}


void setup_start_packet (struct ae_recv * outpkt, int pktcnt, int pktdata, int ackfreq, int ackdata)
{
   struct start_pkt *start_pkt = (struct start_pkt *) ctlpacket;
   outpkt->n = 1;
   outpkt->r[0].sz = max(ETHER_MIN_LEN, sizeof(struct start_pkt));
   outpkt->r[0].data = ctlpacket;
   start_pkt->type = PKT_START;
   start_pkt->pktcnt = pktcnt;
   start_pkt->pktdata = pktdata;
   start_pkt->ackfreq = ackfreq;
   start_pkt->ackdata = ackdata;
}


#define setup_startack_packet(outpkt)	setup_packet(outpkt, PKT_STARTACK, 0)
#define setup_stop_packet(outpkt)	setup_packet(outpkt, PKT_STOP, 0)
#define setup_stopack_packet(outpkt)	setup_packet(outpkt, PKT_STOPACK, 0)
#define setup_data_packet(outpkt)	setup_packet(outpkt, PKT_DATA, pktdata)
#define setup_dataack_packet(outpkt)	setup_packet(outpkt, PKT_DATAACK, ackdata)


void send_packet (int netcardno, struct ae_recv *outpkt)
{
#ifdef DIRECT_XMIT
   int ret = 0;
   while (sys_net_xmit (netcardno, outpkt, NULL, 0) != 0) ;
#else
   int ret = xio_net_wrap_send ((netcardno + 1), outpkt, 64);
#endif
   assert (ret == 0);
}

#define is_stop_pkt(packet)	(((struct pkt *)(packet)->r[0].data)->type == PKT_STOP)


void process_start_packet (struct start_pkt *start_pkt)
{
   pktcnt = start_pkt->pktcnt;
   pktdata = start_pkt->pktdata;
   ackfreq = start_pkt->ackfreq;
   ackdata = start_pkt->ackdata;
printf ("process_start_packet: pktcnt %d, pktdata %d, ackfreq %d, ackdata %d\n", pktcnt, pktdata, ackfreq, ackdata);
   assert (ntohs(((struct udp_pkt *)start_pkt->udp)->udp_dport) == srcportno);
   dstportno = ntohs(((struct udp_pkt *)start_pkt->udp)->udp_sport);
   memcpy((char*)&ip_dstaddr, (char *)&((struct ip_pkt *)start_pkt->ip)->ip_src, sizeof(((struct ip_pkt *)start_pkt->ip)->ip_src));
   memcpy((char*)&ip_myaddr, (char *)&((struct ip_pkt *)start_pkt->ip)->ip_dst, sizeof(((struct ip_pkt *)start_pkt->ip)->ip_src));
   memcpy(&eth_dstaddr, ((struct ether_pkt *)start_pkt->ether)->ether_shost, 6);
   memcpy(&eth_myaddr, ((struct ether_pkt *)start_pkt->ether)->ether_dhost, 6);
}


void do_client ()
{
   struct ae_recv *packet;
   int ret;
   int pktsent = 0, ackrecv = 0;

   init ();
   setnetcardno (eth_myaddr);

	/* set up pktring and get DPF filter for proper UDP packets */
   xio_net_wrap_init (&nwinfo, malloc(32 * 4096), (32 * 4096));
printf ("getting dpf: dstportno %x (%d), srcportno %x (%d)\n", dstportno, dstportno, srcportno, srcportno);
   ret = xio_net_wrap_getdpf_udp (&nwinfo, dstportno, srcportno);
   assert (ret != -1);

   init_packets ();

	/* construct and send start packet */
   setup_start_packet (outpkt, pktcnt, pktdata, ackfreq, ackdata);
printf ("sending packet: netcardno %d, dstportno %02x %02x, srcportno %02x %02x\n", netcardno, outpkt->r[0].data[36], outpkt->r[0].data[37], outpkt->r[0].data[34], outpkt->r[0].data[35]);
   send_packet (netcardno, outpkt);

	/* setup data packet */
   setup_data_packet (outpkt);

	/* get ack.  start timer. */
   //while ((packet = xio_net_wrap_getPacket(&nwinfo)) == NULL) ;
   //xio_net_wrap_returnPacket (&nwinfo, packet);
//
//printf ("got ack\n");

	/* send packets, and wait for acks when appropriate */
   while (pktsent < pktcnt) {
#ifndef DIRECT_XMIT
      ((struct pkt *)outpkt->r[0].data)->id = pktsent;
#endif
      send_packet (netcardno, outpkt);
      pktsent++;
      if ((ackfreq) && ((pktsent % ackfreq) == 0)) {
         while ((packet = xio_net_wrap_getPacket(&nwinfo)) == NULL) ;
         assert (packet->r[0].sz == max(64,(sizeof(struct pkt) + ackdata - 4)));
         xio_net_wrap_returnPacket (&nwinfo, packet);
         ackrecv++;
      }
   }

	/* when sent "pktcnt" packets, get time send stop packet */
   setup_stop_packet (outpkt);
   send_packet (netcardno, outpkt);

   /* print results */
   printf ("packets sent %d (%d acks)\n", pktsent, ackrecv);
}


void do_server ()
{
   struct ae_recv *packet;
   int ret;
   int pktrecv = 0;
   int acksent = 0;
   int gotstop = 0;
   int printmismatch;
   int adjust;

	/* se  up pktring and get DPF filter for proper UDP packets */
   xio_net_wrap_init (&nwinfo, malloc(32 * 4096), (32 * 4096));
printf ("getting dpf: dstportno %x (%d), srcportno %x (%d)\n", dstportno, dstportno, -1, -1);
   ret = xio_net_wrap_getdpf_udp (&nwinfo, dstportno, -1);
   assert (ret != -1);

	/* wait for start packet */
   while ((packet = xio_net_wrap_getPacket(&nwinfo)) == NULL) ;

printf ("got packet: dstportno %02x %02x, srcportno %02x %02x\n", packet->r[0].data[36], packet->r[0].data[37], packet->r[0].data[34], packet->r[0].data[35]);

	/* set up proper variables, send ack packet and start timer */
   process_start_packet ((struct start_pkt *) packet->r[0].data);
   setnetcardno (packet->r[0].data);
   xio_net_wrap_returnPacket (&nwinfo, packet);
   init_packets ();
   setup_startack_packet (outpkt);
printf ("sending packet: netcardno %d, dstportno %02x %02x, srcportno %02x %02x\n", netcardno, outpkt->r[0].data[36], outpkt->r[0].data[37], outpkt->r[0].data[34], outpkt->r[0].data[35]);
   send_packet (netcardno, outpkt);

	/* get Packets, check their size, ack when appropriate */
   printmismatch = 1;
   adjust = 0;
   while (pktrecv < pktcnt) {
      while ((packet = xio_net_wrap_getPacket(&nwinfo)) == NULL) ;
      if (is_stop_pkt (packet)) {
         gotstop = 1;
         break;
      }
#ifndef DIRECT_XMIT
      if ((printmismatch) && (((struct pkt *)packet->r[0].data)->id != (pktrecv+adjust))) {
         //printf ("out of order packet: id %d, expected %d (adjust %d)\n", ((struct pkt *)packet->r[0].data)->id, pktrecv, adjust);
         //printmismatch = 0;
         adjust = ((struct pkt *)packet->r[0].data)->id - pktrecv;
      }
#endif
      assert (packet->r[0].sz == max(64,(sizeof(struct pkt) + pktdata - 4)));
      xio_net_wrap_returnPacket (&nwinfo, packet);
      pktrecv++;
      if ((ackfreq) && ((pktrecv % ackfreq) == 0)) {
         setup_dataack_packet (outpkt);
         send_packet (netcardno, outpkt);
         acksent++;
      }
   }

   /* when stop packet received, get time */

   /* print results */
   printf ("packets received %d (acks %d)   (gotstop %d)\n", pktrecv, acksent, gotstop);
}


void usage(char *argv[])
{
   fprintf(stderr, "usage: %s s <portno>\n", argv[0]);
   fprintf(stderr, "------- or ---------\n");
   fprintf(stderr, "usage: %s c dsthost myhost <pktcnt> <pktdata> [<ackfreq> <ackdata>]\n", argv[0]);
}


int 
main(int argc, char *argv[])
{

   if (argc < 3) {
      usage(argv);
      exit(1);
   }

   isserver = argv[1][0] == 's';
   if (isserver) {
      if (argc != 3) {
         usage(argv);
         exit(1);
      }
      srcportno = atoi (argv[2]);
      netcardno = __sysinfo.si_nnetworks - 1;
      startinpackets = __sysinfo.si_networks[netcardno].rcvs;
      startoutpackets = __sysinfo.si_networks[netcardno].xmits;
      do_server ();

   } else {
      if (argc < 6) {
         usage(argv);
         exit(1);
      }
      host = argv[2];
      myhost = argv[3];
      pktcnt = atoi(argv[4]);
      pktdata = atoi(argv[5]);
      if (argc >= 7) {
         ackfreq = atoi(argv[6]);
      }
      if (argc >= 8) {
         ackdata = atoi(argv[7]);
      }

      do_client ();
   }

   if (netcardno >= 0) {
      printf ("%s done (netin %qd, netout %qd)\n", argv[0], (__sysinfo.si_networks[netcardno].rcvs - startinpackets), (__sysinfo.si_networks[netcardno].xmits - startoutpackets));
   } else {
      printf ("%s done (netcardno %d)\n", argv[0], netcardno);
   }

   return 0;
}

