#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <xuser.h>

#include <exos/netinet/hosttable.h>

static int isserver;
static char *host;
static char *myhost;
static uint32 ip_dstaddr;
static uint32 ip_myaddr;
static char eth_dstaddr[6];
static char eth_myaddr[6];
static int cnt;
static int bandwidth;
static int poll;


/*
 * Simple program to measure minimum (driver-driver) roundtrip times.
 * The interesting stuff is in the kernel.
 * See usage() to see how this program gets invoked.
 */

static inline void
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

    printf("dst: %x ", ip_dstaddr);
    ip_print((char *)&ip_dstaddr);

    fflush(stdout);
}

static void
usage(void)
{
    fprintf(stderr, "tudp: usage lb c/s dst myhost cnt bandwidth p/i\n");
    exit(1);
}


int 
main(int argc, char *argv[])
{

    if (argc != 7) {
	usage();
    }
    
    isserver = argv[1][0] == 's';
    host = argv[2];
    myhost = argv[3];
    cnt = atoi(argv[4]);
    bandwidth = atoi(argv[5]);
    poll = argv[6][0] == 'p';

    init();

    if (argv[1][0] == 's') {
	sys_lb_server(ip_myaddr, ip_dstaddr, eth_myaddr, eth_dstaddr,
		      cnt, bandwidth, poll);
    } else {
	sys_lb_client(ip_dstaddr, ip_myaddr, eth_dstaddr, eth_myaddr,
		      cnt, bandwidth, poll);
    }

    return 0;
}

