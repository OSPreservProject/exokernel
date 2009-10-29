
/*
 * Copyright (C) 1997 Massachusetts Institute of Technology 
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders. See the file AUTHORS which should have accompanied this software
 * for a list of all copyright holders.
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by the copyright
 * holders listed in the AUTHORS file. The rest of this file is covered by
 * the copyright notices, if any, listed below.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/time.h>


/*
 * Test udp.
 *
 * Arguments:  c/s host myport dport cnt len
 */

#define NNAME 128
#define NMSG  1024

typedef unsigned short uint16;
typedef unsigned long uint32;

static int isserver;
static char *host;
static char myhost[NNAME];
static int len;
static uint16 myport;
static uint16 dstport;
static uint32 ip_dstaddr;
static uint32 ip_myaddr;
struct sockaddr_in myaddr;
struct sockaddr_in dstaddr;

static int cnt;
static char msg[NMSG];
static int s;


static void
usage(void)
{
    fprintf(stderr, "tudp: usage tudp c/s host myport dport cnt len\n");
    exit(1);
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

    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    if ((addr=gethostbyname(host)) == NULL) {
	perror("bad hostname");
	exit(1);
    }
    memcpy((char*)&ip_dstaddr, addr->h_addr, addr->h_length);

    dstaddr.sin_family = AF_INET;
    dstaddr.sin_addr.s_addr = ip_dstaddr;
    dstaddr.sin_port = htons (dstport);

    if (gethostname(myhost, NNAME) < 0) {
	perror("gethostname");
	exit(1);
    }

    if ((addr=gethostbyname(myhost)) == NULL) {
	perror("bad local hostname");
	exit(1);
    }
    memcpy((char*)&ip_myaddr, addr->h_addr, addr->h_length);

    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = ip_myaddr;
    myaddr.sin_port = htons(myport);

    if (bind (s, (struct sockaddr *) &myaddr, sizeof (myaddr)) < 0) {
	perror("bind");
	exit(1);
    }

    if (connect (s, (struct sockaddr *) &dstaddr, sizeof (dstaddr)) < 0) 
    {
	perror("connect");
	exit(1);
    }
}


static void
server(void)
{
    int i;
    int r;

    printf("server\n");

    for (i = 0; i < cnt; i++) {
	if ((r = read(s, msg, len)) != len) {
	    fprintf(stderr, "server: read returned %d\n", r);
	    exit(1);
	}
	if ((r = write(s, msg, len)) != len) {
	    fprintf(stderr, "server: write returned %d\n", r);
	    exit(1);
	}
    }
}


static void
client(void)
{
    int i;
    int r;
    struct timeval t1, t2;
    long time;

    printf("client: write %d bytes %d times\n", len, cnt);

    gettimeofday(&t1,NULL);

    for (i = 0; i < cnt; i++) {
	if ((r = write(s, msg, len)) != len) {
	    fprintf(stderr, "server: write returned %d\n", r);
	    exit(1);
	}
	if ((r = read(s, msg, len)) != len) {
	    fprintf(stderr, "server: read returned %d\n", r);
	    exit(1);
	}
    } 
    gettimeofday(&t2, NULL);
    time = (t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_usec - t1.tv_usec);

    printf("Received: %d roundtrip with %d bytes in %d usec\n", cnt,
	   len, time);

}


int 
main(int argc, char *argv[])
{
    if (argc != 7) {
	usage();
    }
    
    isserver = argv[1][0] == 's';
    host = argv[2];
    myport = atoi(argv[3]);
    dstport = atoi(argv[4]);
    cnt = atoi(argv[5]);
    len = atoi(argv[6]);

    init();

    if (isserver) server();
    else client();

    return(0);
}
