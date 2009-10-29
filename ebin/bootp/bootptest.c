/*
 * bootptest.c - Test out a bootp server.
 *
 * This simple program was put together from pieces taken from
 * various places, including the CMU BOOTP client and server.
 * The packet printing routine is from the Berkeley "tcpdump"
 * program with some enhancements I added.  The print-bootp.c
 * file was shared with my copy of "tcpdump" and therefore uses
 * some unusual utility routines that would normally be provided
 * by various parts of the tcpdump program.  Gordon W. Ross
 *
 * Boilerplate:
 *
 * This program includes software developed by the University of
 * California, Lawrence Berkeley Laboratory and its contributors.
 * (See the copyright notice in print-bootp.c)
 *
 * The remainder of this program is public domain.  You may do
 * whatever you like with it except claim that you wrote it.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * HISTORY:
 *
 * 12/02/93 Released version 1.4 (with bootp-2.3.2)
 * 11/05/93 Released version 1.3
 * 10/14/93 Released version 1.2
 * 10/11/93 Released version 1.1
 * 09/28/93 Released version 1.0
 * 09/93 Original developed by Gordon W. Ross <gwr@mc.com>
 */

char *usage = "bootptest [-h] server-name [vendor-data-template-file]";

#define PR kprintf("%s:%d\n",__FILE__,__LINE__)


#include <exos/net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>			/* inet_ntoa */

#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>
#include <assert.h>

#include <unistd.h>

#include "bootp.h"
#include "bootptest.h"
#include "patchlevel.h"

#define LOG_ERR 1
#define BUFLEN 1024
#define WAITSECS 1
#define MAXWAIT  10

int vflag = 1;
int tflag = 0;
int thiszone;
char *progname;
unsigned char *packetp;
unsigned char *snapend;
int snaplen;

/* from getether.c */
extern int getether(char *ifname, char *eaddr);
/* from bootstrap.c */
extern void bootstrap_turnon_interfaces();
extern void bootstrap();

/* at end of this file */
void send_request(int s);

/*
 * IP port numbers for client and server obtained from /etc/services
 */

u_short bootps_port, bootpc_port;


/*
 * Internet socket and interface config structures
 */

struct sockaddr_in sin_server;	/* where to send requests */
struct sockaddr_in sin_client;	/* for bind and listen */
struct sockaddr_in sin_from;	/* Packet source */
u_char eaddr[16];				/* Ethernet address */

/*
 * General
 */

#ifndef DEBUG 
#define DEBUG 0
#endif

int debug = DEBUG;					/* Debugging flag (level) */
/* there is another debug in bootstrap.c  */
char hostname[64];
static char sndbufspace[BUFLEN];
char *sndbuf = sndbufspace;					/* Send packet buffer */
static char rcvbufspace[BUFLEN];
char *rcvbuf = rcvbufspace;					/* Receive packet buffer */

/*
 * Vendor magic cookies for CMU and RFC1048
 */

unsigned char vm_cmu[4] = VM_CMU;
unsigned char vm_rfc1048[4] = VM_RFC1048;
short secs;						/* How long client has waited */

char *get_errmsg();
extern void bootp_print();



/*
 * Initialization such as command-line processing is done, then
 * the receiver loop is started.  Die when interrupted.
 */

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct bootp *bp;



	char *servername = NULL;
	int s;				/* Socket file descriptor */
	int n, fromlen, recvcnt;
	int32_t vend_magic;
	int32_t xid;
	char interface_name[IFNAMSZ];

	if (debug)
		printf("%s: version %s.%d\n", progname, VERSION, PATCHLEVEL);

	/*
	 * Verify that "struct bootp" has the correct official size.
	 * (Catch evil compilers that do struct padding.)
	 */
	assert(sizeof(struct bootp) == BP_MINPKTSZ);

	/* default magic number */
	bcopy(vm_rfc1048, (char*)&vend_magic, 4);

	/*
	 * Create a socket.
	 */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	/*
	 * Set up server socket address (for send)
	 */

	/* set broadcast address */
	bootps_port = (u_short) IPPORT_BOOTPS;
	memset(&sin_server.sin_addr, 0xff,sizeof(sin_server.sin_addr));
	sin_server.sin_family = AF_INET;
	sin_server.sin_port = htons(bootps_port);


	/*
	 * Set up client socket address (for listen)
	 */
	bootpc_port = (u_short) IPPORT_BOOTPC;
	sin_client.sin_family = AF_INET;
	sin_client.sin_port = htons(bootpc_port);
	sin_client.sin_addr.s_addr = INADDR_ANY;

	/*
	 * Bind client socket to BOOTPC port.
	 */
	if (bind(s, (struct sockaddr *) &sin_client, sizeof(sin_client)) < 0) {
		perror("bind BOOTPC port");
		if (errno == EACCES)
			fprintf(stderr, "You need to run this as root\n");
		exit(1);
	}
	/*
	 * Build a request.
	 */
	bp = (struct bootp *) sndbuf;
	bzero(bp, sizeof(*bp));
	bp->bp_op = BOOTREQUEST;
	xid = (int32_t) getpid();
	bp->bp_xid = (u_int32_t) htonl(xid);
	
	/*
	 * Fill in the hardware address (or client IP address)
	 */
	if (getether(interface_name, eaddr)) {
	  printf("Can not get ether addr for %s\n", interface_name);
	  exit(1);
	}
	/* Copy Ethernet address into request packet. */
	bp->bp_htype = 1;
	bp->bp_hlen = 6;
	if (debug) printf("eaddr: %x %x %x %x %x %x\n",
					  eaddr[0],eaddr[1],eaddr[2],eaddr[3],eaddr[4],eaddr[5]);
	bcopy(eaddr, bp->bp_chaddr, bp->bp_hlen);

	/*
	 * Copy in the default vendor data.
	 */
	bcopy((char*)&vend_magic, bp->bp_vend, 4);
	if (vend_magic)
		bp->bp_vend[4] = TAG_END;


	/*
	 * Set globals needed by print_bootp
	 * (called by send_request)
	 */
	snaplen = sizeof(*bp);
	packetp = (unsigned char *) eaddr;
	snapend = (unsigned char *) sndbuf + snaplen;

	/* Send a request once per second while waiting for replies. */
	recvcnt = 0;
	bp->bp_secs = secs = 0;

	kprintf("ENABLING INTERFACES\n");
	bootstrap_turnon_interfaces();

	send_request(s);
	while (1) {
		struct timeval tv;
		int readfds;

		tv.tv_sec = WAITSECS;
		tv.tv_usec = 0L;
		readfds = (1 << s);
		n = select(s + 1, (fd_set *) & readfds, NULL, NULL, &tv);
		if (n < 0) {
			perror("select");
			break;
		}
		if (n == 0) {
			/*
			 * We have not received a response in the last second.
			 * If we have ever received any responses, exit now.
			 * Otherwise, bump the "wait time" field and re-send.
			 */
			if (recvcnt > 0)
				exit(0);
			secs += WAITSECS;
			if (secs > MAXWAIT)
				break;
			bp->bp_secs = htons(secs);
			send_request(s);
			continue;
		}
		fromlen = sizeof(sin_from);
		n = recvfrom(s, rcvbuf, BUFLEN, 0,
					 (struct sockaddr *) &sin_from, &fromlen);
		if (n <= 0) {
			continue;
		}
		if (n < sizeof(struct bootp)) {
			printf("received short packet\n");
			continue;
		}
		recvcnt++;

		/* Print the received packet. */
		printf("Recvd from %s", inet_ntoa(sin_from.sin_addr));
		/* set globals needed by bootp_print() */
		snaplen = n;
		snapend = (unsigned char *) rcvbuf + snaplen;

		/* Make sure this is our packet and not some astray broadcast packet */
		{
		  unsigned char *eaddr_recv;
		  struct bootp *bp = (struct bootp *)rcvbuf;
		  eaddr_recv = (char *)&bp->bp_chaddr;

		  if (bcmp(eaddr_recv,eaddr,6) != 0) {
			printf("Sent ethernet address and received address differ, skipping\n");
			continue;
		  }


		}
		if (debug) {
		  bootp_print(rcvbuf, n, sin_from.sin_port, 0);
		  bootstrap(interface_name,rcvbuf,n);
		  printf("BOOTSTRAP\n");
		  putchar('\n');
		} else {
		  bootstrap(interface_name,rcvbuf,n);
		  /* NOT REACHED */
		}
		/*
		 * This no longer exits immediately after receiving
		 * one response because it is useful to know if the
		 * client might get multiple responses.  This code
		 * will now listen for one second after a response.
		 */
	}
	fprintf(stderr, "no response from %s\n", servername);
	exit(1);
}

void
send_request(s)
	int s;
{
	/* Print the request packet. */
	printf("Sending to %s", inet_ntoa(sin_server.sin_addr));

	bootp_print(sndbuf, snaplen, sin_from.sin_port, 0);

	putchar('\n');

	/* Send the request packet. */
	if (sendto(s, sndbuf, snaplen, 0,
			   (struct sockaddr *) &sin_server,
			   sizeof(sin_server)) < 0)
	{
		perror("sendto server");
		exit(1);
	}
}

/*
 * Print out a filename (or other ascii string).
 * Return true if truncated.
 */
int
printfn(s, ep)
        register u_char *s, *ep;
{
        register u_char c;

        putchar('"');
        while (c = *s++) {
                if (s > ep) {
                        putchar('"');
                        return (1);
                }
                if (!isascii(c)) {
                        c = toascii(c);
                        putchar('M');
                        putchar('-');
                }
                if (!isprint(c)) {
                        c ^= 0x40;                      /* DEL to ?, others to a
lpha */
                        putchar('^');
                }
                putchar(c);
        }
        putchar('"');
        return (0);
}


/*
 * Convert an IP addr to a string.
 * (like inet_ntoa, but ina is a pointer)
 */
char *
ipaddr_string(ina)
        struct in_addr *ina;
{
        static char b[24];
        u_char *p;

        p = (u_char *) ina;
        sprintf(b, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
        return (b);
}


/*
 * Local Variables:
 * tab-width: 4
 * c-indent-level: 4
 * c-argdecl-indent: 4
 * c-continued-statement-offset: 4
 * c-continued-brace-offset: -4
 * c-label-offset: -4
 * c-brace-offset: 0
 * End:
 */
