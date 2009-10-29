#include <assert.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <net/tcp/types.h>
#include <net/tcp/libtcp.h>

#define NBPG 4096

#define NONBLOCKING

static char *server;

struct hostinfo {
    char *name;
    char eth_addr[6];
    char ip_addr[4];
} an2_hosttable_map[] = {
    {"dexter", {0x0, 0x0, 0xc0, 0xc9, 0xa5, 0x4e}, {18,26,4,17}},
    {"amsterdam", {0x8, 0x0, 0x20, 0x73, 0xb9, 0x1c}, {18,26,0,9}},
    {"redlab", {0x0, 0x0, 0xc0, 0xd0, 0xa5, 0x4e}, {18, 26, 0, 144}},
    {"dua", {0x0,0x0,0xc0,0x6e,0x5f,0xd0}, {18,26,4,19}},
    {"isca", {0x0,0x0,0xc0,0xc4,0x78,0xb8}, {18,26,4,25}},
    {"cone", {0, 0x0, 0xc0, 0xf4, 0x3c, 0xab}, {18,26,4,24}},
    {"cbn", {0x0,0x0,0xc0,0xd0,0xa5,0x4e}, {18,26,4,21}},
    {"whap", {0x0,0x0,0xc0,0x3,0x48,0xd7}, {18,26,4,28}},
    {"pnp", {0x0,0x0,0xc0,0xac,0x3c,0xab}, {18,26,4,26}},
    {"strugion", {0x0,0x0,0xc0,0x3f,0x4e,0xd7}, {18,26,4,27}},
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

/* #define INITIAL 	/* if first and only process on exokernel */

/*
 * Set of test function for the tcplib. This is the client side.
 */

#define NTEST 		100
#define N	        1024 * 32
#define MAXLISTEN	5

static char initial_msg[NTEST];
static char msg_array[N+NBPG];
static int msg_n = N+NBPG;  /* should match size of msg_array */
static char *msg;  /* will be set to point into msg_array */
#ifdef INPLACE
static struct ae_recv arecv;
#endif
static uint32 ip_src;
static char *eth_src;
static uint16 src_port;
static void *listen_handle;

#define HTTP_PORT      	80

#ifndef INPLACE
/* Test reading */
static void 
test1(void *handle)
{
    int len;

    while (1) {
	if ((len = tcp_read(handle, msg, msg_n)) < 0) {
	    printf("test1: tcp_read failed %d\n", len);
	}

	if (len > 0) printf("test1: %s\n", msg);
	else {
	    return;
	}
    }
}
#endif

/* Test writing */
static void 
test2(void *handle)
{
    int len;
    int i;

    memcpy(msg, "  hello world", strlen("  hello world") + 1);

    for (i = 0; i < 2; i++) {
	msg[0] = '0' + i % 10;
	
	printf("test2: %d bytes\n", strlen(msg) + 1);

	if ((len = tcp_write(handle, msg, strlen(msg) + 1)) < 0) {
	    printf("test2: tcp_write failed %d\n", len);
	    return;
	}
    }
}


/* Test large writing */
static void 
test3(void *handle, int len, int n)
{
    printf("send %d bytes in %d byte segments\n", len, n);

    do {
       if (n != tcp_write(handle, msg, n)) {
	   printf("test3: tcp_write failed\n");
	   return;
       }
       len -= n;
   } while (len > 0);
}


#ifndef INPLACE
/* Test large reading */
static void 
test4(void *handle)
{
    int len;
    int total;

    total = 0;
    while (1) {
	if ((len = tcp_read(handle, msg, msg_n)) < 0) {
	    printf("test4: tcp_read failed %d\n", len);
	}

	if (len <= 0) break;
	
	total += len;
    }
}
#endif

/* Test latency */
static void
test10(void *handle)
{
    int n;
    int len;

    n = sizeof(int);

    if ((len = tcp_write(handle, msg, n)) < 0) {
	printf("test10: tcp_write failed %d\n", len);
    }

    while (1) {
#ifndef INPLACE
	if ((len = tcp_read(handle, msg, n)) < 0) {
#else
	if ((len = tcp_read(handle, &arecv, n)) < 0) {
#endif
	    printf("test10: tcp_read failed %d\n", len);
	}
#ifdef INPLACE
	if (len != 0) tcp_return(&arecv);
#endif

	if (len <= 0) break;

#ifdef NONBLOCKING
	if ((len = tcp_non_blocking_write(handle, msg, n, 0)) < 0) {
	    printf("test10: tcp_write failed %d\n", len);
	}
#else
	if ((len = tcp_write(handle, msg, n)) < 0) {
	    printf("test10: tcp_write failed %d\n", len);
	}
#endif
    }
#ifdef TIMING
tprint();
#endif
}


static void
test20(void *handle)
{
}


void 
test(void)
{
    void *handle;
    int n;
    int len;
    int s;

    if ((handle = (void *) tcp_accept(listen_handle)) == 0)
    {
	printf("test: tcp_connect failed\n");
	return;
    }

#ifndef INPLACE
    if ((len = tcp_read(handle, initial_msg, 2*sizeof(int))) < 0) {
#else
    if ((len = tcp_read(handle, &arecv, 2*sizeof(int))) < 0) {
#endif
       printf("test: read failed %d\n", len);
       exit(1);
    }
#ifndef INPLACE
    memcpy(&n, initial_msg, sizeof(int));
    memcpy(&s, initial_msg + sizeof(int), sizeof(int));
#else
    memcpy(&n, arecv.r[0].data, sizeof(int));
    memcpy(&s, arecv.r[0].data + sizeof(int), sizeof(int));
    tcp_return(&arecv);
#endif
    n = ntohl(n);
    s = ntohl(s);
    printf("test %d size %d\n", n, s);

    switch(n) {
#ifndef INPLACE
    case 1:
	test1(handle);
	break;
#endif
    case 2:
	test2(handle);
	break;
    case 3:
	test3(handle, 1000 * 1024, s);
	break;
#ifndef INPLACE
    case 4:
	test4(handle);
	break;
#endif
    case 6:
	test3(handle, 100 * 1024, s);
	break;
    case 10:
	test10(handle);
	break;
    case 20:
	test20(handle);
	break;
    default:
	printf("illegal opcode %d\n", n);
	exit(0);
    }

    if (tcp_close(handle) < 0) {
	printf("test: tcp_close failed\n");
    }

    printf("run test %d done\n", n);
}

#ifdef INITIAL
int
main(void)
#else
int
main (int argc, char *argv[])
#endif
{
    int i;

#ifdef INITIAL
    dst_port = 3333;
#else
    if (argc < 3) {
	printf("usage %s server server-port\n", argv[0]);
	return(0);
    }

    src_port = atoi(argv[2]);
    server = argv[1];
#endif

    /* align msg data */
    msg = msg_array;
    msg_n = N + NBPG;
    if(((unsigned)msg_array%NBPG)!=0) {
	msg = (void *)(((unsigned)msg_array+NBPG)&~(NBPG-1));
	msg_n -= NBPG;
    }

    for(i=0; i<msg_n; i++) msg[i] = i&0xff;  /* initialize array */

    printf("server port is %d, server is %s\n", src_port, server);

    memcpy(&ip_src, 
	   an2_hosttable_map[get_map_index_by_name(server)].ip_addr, 4);
    eth_src = an2_hosttable_map[get_map_index_by_name(server)].eth_addr;

    /* initialize tcp library */
    tcp_init();

    if ((listen_handle = tcp_listen(MAXLISTEN,
				    0, 0, 0, src_port, ip_src, eth_src)) == 0) {
	printf("listen failed\n");
	exit(1);
    }

    while (1) {
	test();
#if 0
	tcp_statistics();
#endif
    }

    tcp_close(listen_handle);


    exit(0);
}

/**********************************************************************/

