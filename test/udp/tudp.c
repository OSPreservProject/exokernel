#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <net/udp/udp_eth.h>
#include <net/udp/udp2_eth.h>
#include <xuser.h>
#include <sys/pctr.h>
#include <net/tcp/libtcp.h>
#include <net/tcp/tcp.h>

#include <exos/netinet/hosttable.h>

/*
 * Test udp.
 *
 * Arguments:  c/s host myport dport cnt len [ myhost tcp? ]
 */

#define NNAME 128
#define NMSG  1024

static int isserver;
static int tcp = 0;	/* UDP by default */
static char *host;
static char myhostbuf[NNAME];
static char *myhost = NULL;
static int len;
static uint16 myport;
static uint16 dport;
static struct udp_session s;
static struct udp2_session s2;
static struct tcb *t;
static uint32 ip_dstaddr;
static uint32 ip_myaddr;
static char eth_dstaddr[6];
static char eth_myaddr[6];
static int cnt;
static char msg[NMSG];
static unsigned int rate;

unsigned int ae_getrate();
unsigned int ae_gettick();


static void
usage(void)
{
    fprintf(stderr, "tudp: usage tudp c/s host myport dport cnt len [ myhost tcp? ]\n");
    exit(1);
}


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

    rate = ae_getrate();

    if ((addr=gethostbyname(host)) == NULL) {
	perror("bad hostname");
	exit(1);
    }
    memcpy((char*)&ip_dstaddr, addr->h_addr, addr->h_length);

    if (myhost == NULL) {
       if (gethostname(myhostbuf, NNAME) < 0) {
          perror("gethostname");
          exit(1);
       }
       myhost = &myhostbuf[0];
    }

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
    eth_print(eth_myaddr);

    printf("dst: %x ", ip_dstaddr);
    ip_print((char *)&ip_dstaddr);
    eth_print(eth_dstaddr);

    if (tcp == 0) {
        udp_open(&s, dport, myport, ip_dstaddr, eth_dstaddr, ip_myaddr, eth_myaddr);

        printf ("UDP connection setup: s.netcardno %d\n", s.netcardno);

    } else if (tcp == 2) {
        udp2_open(&s2, dport, myport, ip_dstaddr, eth_dstaddr, ip_myaddr, eth_myaddr);

        printf ("UDP2 connection setup: s2.netcardno %d\n", s2.netcardno);

    } else if (isserver) {
        struct tcb *l = tcp_listen(1, dport, ip_dstaddr, eth_dstaddr, myport, ip_myaddr, eth_myaddr);

        t = tcp_accept (l);

        printf ("TCP connection setup: t->netcardno %d\n", t->netcardno);

    } else {
        t = tcp_connect (dport, ip_dstaddr, eth_dstaddr, myport, ip_myaddr, eth_myaddr);

        printf ("TCP connection setup: t->netcardno %d\n", t->netcardno);

    }
}


static void
udp_server(void)
{
    int i;
    int r;

    printf("UDP server\n");

    for (i = 0; i < cnt; i++) {
	if ((r = udp_read(&s, msg, len)) != len) {
	    fprintf(stderr, "UDP server: read returned %d\n", r);
	    exit(1);
	}
	if ((r = udp_write(&s, msg, len)) != len) {
	    fprintf(stderr, "UDP server: write returned %d\n", r);
	    exit(1);
	}
    }
}


static void
udp2_server(void)
{
    int i;
    int r;

    printf("UDP2 server\n");

    for (i = 0; i < cnt; i++) {
	if ((r = udp2_read(&s2, msg, len)) != len) {
	    fprintf(stderr, "UDP2 server: read returned %d\n", r);
	    exit(1);
	}
	if ((r = udp2_write(&s2, msg, len)) != len) {
	    fprintf(stderr, "UDP2 server: write returned %d\n", r);
	    exit(1);
	}
    }
}


static void
tcp_server(void)
{
    int i;
    int r;

    printf("TCP server\n");

    for (i = 0; i < cnt; i++) {
	if ((r = tcp_read(t, msg, len)) != len) {
	    fprintf(stderr, "TCP server: read returned %d\n", r);
	    exit(1);
	}
	if ((r = tcp_write(t, msg, len)) != len) {
	    fprintf(stderr, "TCP server: write returned %d\n", r);
	    exit(1);
	}
    }
}


static void
client(void)
{
    int i;
    int r;
    int t1, t2;
    pctrval v1;
    pctrval v2;
    pctrval v;

    printf("client: write %d bytes %d times\n", len, cnt);

    t1 = ae_gettick();
    v1 = rdtsc();
    if (tcp == 0) {
        for (i = 0; i < cnt; i++) {
	    if ((r = udp_write(&s, msg, len)) != len) {
	        fprintf(stderr, "UDP client: write returned %d\n", r);
	        exit(1);
	    }
	    if ((r = udp_read(&s, msg, len)) != len) {
	        fprintf(stderr, "UDP client: read returned %d\n", r);
	        exit(1);
	    }
        } 
    } else if (tcp == 2) {
        for (i = 0; i < cnt; i++) {
	    if ((r = udp2_write(&s2, msg, len)) != len) {
	        fprintf(stderr, "UDP2 client: write returned %d\n", r);
	        exit(1);
	    }
	    if ((r = udp2_read(&s2, msg, len)) != len) {
	        fprintf(stderr, "UDP2 client: read returned %d\n", r);
	        exit(1);
	    }
        } 
    } else {
        for (i = 0; i < cnt; i++) {
	    if ((r = tcp_write(t, msg, len)) != len) {
	        fprintf(stderr, "TCP client: write returned %d\n", r);
	        exit(1);
	    }
	    if ((r = tcp_read(t, msg, len)) != len) {
	        fprintf(stderr, "TCP client: read returned %d\n", r);
	        exit(1);
	    }
        } 
    }
    v2 = rdtsc();
    t2 = ae_gettick();


    v = v2 - v1;

    printf("time: %X %X\n", (unsigned int) (v & 0xFFFFFFFF), 
	   (unsigned int) ((v >> 32) & 0xFFFFFFFF));

    printf("Received: %d roundtrip with %d bytes in %u usec %d ticks\n", cnt,
	   len, (t2 - t1) * rate, t2 - t1);

}


int 
main(int argc, char *argv[])
{
    if ((argc != 7) && (argc != 8) && (argc != 9)) {
	usage();
    }
    
    isserver = argv[1][0] == 's';
    host = argv[2];
    myport = atoi(argv[3]);
    dport = atoi(argv[4]);
    cnt = atoi(argv[5]);
    len = atoi(argv[6]);
    if (argc >= 8) {
       myhost = argv[7];
    }
    if (argc == 9) {
       tcp = atoi(argv[8]);
    }

    init();

    if (isserver) {
        if (tcp == 0) {
           udp_server();
        } else if (tcp == 2) {
           udp2_server();
        } else {
           tcp_server();
        }
    } else {
        client();
    }

    return(0);
}
