
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

#include <assert.h>
#include <xuser.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <os/tick.h>
#include <net/tcp/types.h>
#include <net/tcp/libtcp.h>


#define NBPG 4096

/* #define NONBLOCKING /* nonblocking */


struct hostinfo {
    char *name;
    char eth_addr[6];
    char ip_addr[4];
} an2_hosttable_map[] = {
    {"dexter", {0x0, 0x0, 0xc0, 0xc9, 0xa5, 0x4e}, {18,26,4,17}},
    {"amsterdam", {0x8, 0x0, 0x20, 0x73, 0xb9, 0x1c}, {18,26,0,9}},
    {}
};


int get_map_index_by_name(unsigned char *name)
{
    int i;

    for (i = 0; an2_hosttable_map[i].name != 0; i++) {
	if (strcmp(name, an2_hosttable_map[i].name) == 0)
	    return i;
    }
    assert(0);
}


#define CLIENTN "dexter"
#define SERVERN "amsterdam"

#define CLIENT get_map_index_by_name(CLIENTN)
#define SERVER get_map_index_by_name(SERVERN)

#define INITIAL	/* if first and only process on exokernel */

/*
 * Set of test function for the tcplib. This is the client side.
 */

#define NTEST 		100
#define N	        1024 * 32

static char msg_array[N+NBPG] = "  hello world";
static int msg_n = N+NBPG;  /* should match size of msg_array */
static char *msg;  /* will be set to point into msg_array */
static uint32 ip_dst;
static uint32 ip_src;
static char *eth_dst;
static char *eth_src;
static uint16 dst_port;
static uint16 src_port;
static unsigned int rate;


void
test1(void *handle)
{
    int i;
    int n;

    for (i = 0; i < 2; i++) {
	msg[0] = '0' + i % 10;
	if ((n = tcp_write(handle, msg, strlen(msg) + 1)) < strlen(msg) + 1) {
	    printf("test1: tcp_write returned %d\n", n);
	}
    }
}


#ifndef INPLACE
void 
test2(void *handle)
{
    int i;
    int n;
    int total = 0;

    printf("start test2\n");

    for (i = 0; i < 2; i++) {
	n = tcp_read(handle, msg, msg_n);
	if (n < 0) {
	    printf("test2: read failed %d\n", n);
	    break;
	}
	for (i = 0; i < n; i++) {
	    printf("%c", msg[i]);
	}
	total += n;
    }

    printf("Received %d bytes\n", total);
}
#endif

void 
test3(void *handle, int size)
{
    int n;
    int total;
    unsigned int s, t;
    total = 0;
    s = ae_gettick();
    do {
	n = tcp_read(handle, msg, msg_n);
#ifdef TESTDATA
{
    int j;
    for(j=0; j<n; j++) {
	if(*((unsigned char *)msg +j) != (count&0xff)) {
	    printf("%d: %p %d %d %d\n", n, msg, j, 
		   *((unsigned char *)msg + j), (count&0xff));
	}
	count++;
    }
}
#endif
	if (n < 0) {
	    printf("test3: read failed %d\n", n);
	    break;
	}
	total += n;

    } while (n > 0);
    t = ae_gettick();

    printf("Received: %d bytes in %u usec %d ticks\n", total, (t - s) * rate,
	   t - s);
}


void 
test4(void *handle, int total)
{
    int m;
    unsigned int s, t;
    unsigned int usec;
    int n;

    s = ae_gettick();
    m = 0;
    do {
	n = tcp_write(handle, msg, msg_n);
	if (n < msg_n) {
	    printf("tcp_write returned %d\n", n);
	    break;
	}
	m += n;
    } while (m < total);
    t = ae_gettick();

    usec = (t - s) * rate;
    printf("Wrote %d bytes in %u usec\n", total, usec);
}


void 
test5(void *handle, void *handle1, int total)
{
    int m;
    unsigned int s, t;
    unsigned int usec;
    int n;

    s = ae_gettick();
    m = 0;
    do {
	n = tcp_write(handle, msg, msg_n);
	if (n < msg_n) {
	    printf("tcp_write returned %d\n", n);
	    break;
	}
	m += n;
	n = tcp_write(handle1, msg, msg_n * 2);
	if (n < msg_n * 2) {
	    printf("tcp_write returned %d\n", n);
	    break;
	}
    } while (m < total);
    t = ae_gettick();

    usec = (t - s) * rate;
    printf("Wrote %d bytes in %u usec\n", total, usec);
}


void
test10(void *handle, int total)
{
    int i;
    int m;
    int n = sizeof(int);
    unsigned int s, t;
    unsigned int usec;


    if ((m = tcp_read(handle, msg, n)) != n) {
	printf("test10: tcp_read returned %d\n", m);
	return;
    }

    s = ae_gettick();

    for (i = 0; i < total; i++) {

#ifdef NONBLOCKING
	if ((m = tcp_non_blocking_write(handle, msg, n, 0)) < 1) {
	    printf("test10: tcp_write returned %d\n", m);
	    return;
	}
#else
	if ((m = tcp_write(handle, msg, n)) != n) {
	    printf("test10: tcp_write returned %d\n", m);
	    return;
	}
#endif

	if ((m = tcp_read(handle, msg, n)) != n) {
	    printf("test10: tcp_read returned %d\n", m);
	    return;
	}

    }

    t = ae_gettick();
    usec = (t - s) * rate;
    printf("%d roundtrips in %u usec\n", total, usec);
}


void 
test(int test, int size)
{
    void *handle;
    void *handle1;
    int n = htonl(test);
    int s = htonl(size);
    int m;

    if ((handle = (void *) tcp_connect(dst_port, ip_dst, eth_dst, src_port, 
				       ip_src, eth_src)) == 0)
    {
	printf("test: tcp_connect failed\n");
	return;
    }

    memcpy(msg, &n, sizeof(int));
    memcpy(msg + sizeof(int), &s, sizeof(int));
    if ((m = tcp_write(handle, msg, 2*sizeof(int))) != 2*sizeof(int)) {
	printf("test: tcp_write returned %d\n", m);
	return;
    }
    printf("run test %d size %d\n", test, size);

    switch(test) {
    case 1:
	test1(handle);
	break;
#ifndef INPLACE
    case 2:
	test2(handle);
	break;
#endif
    case 3:
	test3(handle, size);
	break;
    case 4:
	test4(handle, size);
	break;
    case 5:
	tcp_set_droprate(handle, 20);
	test4(handle, size);
	break;
    case 6:
	tcp_set_droprate(handle, 20);
	test3(handle, size);
	break;
#ifndef UNIX
    case 7:
	if (tcp_close(handle) < 0) {
	  printf("test: tcp_close failed\n");

	}
	test3(handle, size);
	printf("run test %d done\n", test);
	return;
    case 8:
	test4(handle, size);
	printf("run test %d done\n", test);
	break;
    case 9:
	if ((handle1 = (void *) tcp_connect(dst_port, ip_dst, eth_dst, 0,
					    ip_src, eth_src)) == 0)
	{
	    printf("test: tcp_connect for handle 1 failed\n");
	    break;
	}
	
	if ((m = tcp_write(handle1, &n, sizeof(n))) != sizeof(n)) {
	    printf("test: tcp_write returned %d\n", m);
	    break;
	}
	
	test5(handle, handle1, size);
	
	if (tcp_close(handle1) < 0) {
	    printf("test9: close failed\n");
	}
	
	break;
#endif
    case 10:
	test10(handle, size);
	break;
    default:
	printf("test: illegal test %d\n", n);
	break;
    }
    
    if (tcp_close(handle) < 0) {
	printf("test: tcp_close failed\n");
    }

    tcp_exit(handle);

    printf("run test %d done\n", test);
}


#ifndef INPLACE
void 
test_http(void)
{
    void *handle;
    int n;
    int total;

    printf("start http\n");

#define HTTP_PORT      	80

    total = 0;
    if ((handle = (void *) tcp_connect(HTTP_PORT, ip_dst, eth_dst, 0, ip_src,
				    eth_src)) == 0)
    {
	printf("tcp_connect: failed\n");
    }

#define HTTP_REQUEST 	"GET /\n"

    memcpy(msg, HTTP_REQUEST, strlen(HTTP_REQUEST) + 1);

    if (tcp_write(handle, msg, strlen(HTTP_REQUEST) + 1) < 
	strlen(HTTP_REQUEST) + 1) {
	printf("test_http: write failed\n");
    }

    do {
	n = tcp_read(handle, msg, msg_n);
	if (n < 0) {
	    printf("http_test: tcp_read failed %d\n", n);
	    break;
	}
#ifdef 0
	for(i = 0; i < n; i++) {
	    printf("%c", msg[i]);
	}
#endif
	total += n;

    } while (n > 0);

    printf("received: %d bytes\n", total);

    if (tcp_close(handle) < 0) {
	printf("test4: tcp_close failed\n");
    }

    printf("done http\n");
}
#endif

#ifdef INITIAL
int
main(void)
#else
int
main (int argc, char *argv[])
#endif
{
    int i;

    rate = ae_getrate();

#ifdef INITIAL
    src_port = 3333;
    dst_port = 6544;
#else
    if (argc < 3) {
	printf("usage %s client-port server-port\n", argv[0]);
	return(0);
    }

    src_port = atoi(argv[1]);
    dst_port = atoi(argv[2]);
#endif

    /* align msg data */
    msg = msg_array;
    msg_n = N + NBPG;
    if(((unsigned)msg_array%NBPG)!=0) {
	msg = (void *)(((unsigned)msg_array+NBPG)&~(NBPG-1));
	msg_n -= NBPG;
    }

    for(i=0; i<msg_n; i++) msg[i] = i&0xff;  /* initialize array */

    if(src_port != 0) printf("ignoring requested client port\n");
    src_port = 0;
    printf("client port is %d (client %s)\n server port (server %s) is %d\n", 
	   src_port, CLIENTN, SERVERN, dst_port);

    memcpy(&ip_dst, an2_hosttable_map[SERVER].ip_addr, 4);
    eth_dst = an2_hosttable_map[SERVER].eth_addr;

    memcpy(&ip_src, an2_hosttable_map[CLIENT].ip_addr, 4);
    eth_src = an2_hosttable_map[CLIENT].eth_addr;

    printf("init tcp library\n");

    /* initialize tcp library */
    tcp_init();

    for (i = 0; i < NTEST; i++) {
	test(10, 1000);
	test(3, 1024 * 1000);
	test(4, 1024 * 1000);
#if 0
	Test(10, 1000);
	test(3, 1024 * 16);
	tcp_statistics();
	test(3, 1024 * 24);
	tcp_statistics();
	test(3, 1024 * 4);
	tcp_statistics();
	test(2);
	test(3);
	test(4);
	test(5);
	test(6);
	test(7);
	test(8);
	test(9);
	test_http();
#endif
    }
    return 1;
}

