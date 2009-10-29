
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

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/file.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>

#define  MAX_MSG	1024 * 32
#define  N	        1024 * 8
#define  NTEST          1000

/*
 * A hacked up client to test exokernel tcp code.
 */

static char 			*progname;
static char 			msg[MAX_MSG] = "  hello world";
static struct sockaddr_in 	to;


unsigned long gettime()
{
    return time(0);
}


u_long resolve_name( namep, canonp, canonl )
    char *namep, canonp[] ;
    int  canonl ;
{
    struct hostent *hp ;
    u_long inetaddr ;
    int n ;
    
    if (isdigit(*namep)) {
        /* Assume dotted-decimal */
        inetaddr = (u_long) inet_addr(namep) ;
        *canonp = '\0' ;   /* No canonical name */
        return(inetaddr) ;
    }
    else  {
        if (NULL == (hp =  gethostbyname( namep )))
            return(0) ;
        n = ((n = strlen(hp->h_name)) >= canonl) ?  canonl-1 : n ;
        bcopy(hp->h_name, canonp, n) ;
        hp->h_name[n] = '\0' ;
        
        return( *( u_long *) hp->h_addr) ;
    }
}


void 
test1(int s)
{
    int i;
    int n;

    for (i = 0; i < 2; i++) {
	msg[0] = '0' + i % 10;
	if ((n = write(s, msg, strlen(msg) + 1)) < strlen(msg) + 1) {
	    printf("test1: write returned %d\n", n);
	}
    }
}


void 
test2(int s)
{
    int i;
    int n;
    int total = 0;

    printf("start test2\n");

    for (i = 0; i < 2; i++) {
	n = read(s, msg, N);
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


void 
test3(int s)
{
    int i;
    int n;
    int total;
    unsigned int t1, t2;
    unsigned int usec;
 
    total = 0;
    t1 = gettime();
    do {
	n = read(s, msg, N);
	if (n < 0) {
	    printf("test3: read failed %d\n", n);
	    break;
	}
	total += n;

    } while (n > 0);
    t2 = gettime();

    printf("Received: %d bytes in %u usec\n", total, t2 - t1);
}


void 
test4(int s, int total)
{
    int i;
    int m;
    unsigned int t1, t2;
    unsigned int usec;
    int n;

    t1 = gettime();
    m = 0;
    do {
	n = write(s, msg, N);
	if (n < N) {
	    printf("write returned %d\n", n);
	    break;
	}
	m += n;
    } while (m < total);
    t2 = gettime();

    usec = t2 - t1;
    printf("Wrote %d bytes in %u usec\n", total, usec);
}



void 
test10(int s, int n)
{
    int i;
    int r;

    if ((r = read(s, msg, sizeof(int))) != sizeof(int)) {
	printf("test10: read returned %d\n", r);
	return;
    }

    for (i = 0; i < n; i++) {
	if ((r = write(s, msg, sizeof(int))) != sizeof(int)) {
	    printf("test10: write returned %d\n", r);
	    return;
	}
	r  = read(s, msg, sizeof(int));
	if (r != sizeof(int)) {
	    printf("test10: read returned %d\n", r);
	    return;
	}
    }
}


void 
test20(int s, int n)
{
    int i;
    int r;
    int s1;
    int len;
    int on;


    if ((s1 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	perror("socket");
	exit(1);
    }
    
    on = 1;
    if (setsockopt(s1,  SOL_SOCKET, SO_DEBUG, &on, sizeof(on)) < 0) {
	perror("setsockopt");
	exit(1);
    }

    len = sizeof(to);

    if (connect(s1, (struct sockaddr *) &to, len) < 0) {
	perror("connect");
	exit(1);
    }

    if ((r = read(s, msg, sizeof(int))) != sizeof(int)) {
	printf("test10: read returned %d\n", r);
	return;
    }

    for (i = 0; i < n; i++) {
	if ((r = write(s, msg, sizeof(int))) != sizeof(int)) {
	    printf("test10: write returned %d\n", r);
	    return;
	}

	if ((r = write(s1, msg, 2 * sizeof(int))) != 2 * sizeof(int)) {
	    printf("test10: write s1 returned %d\n", r);
	    return;
	}

	r  = read(s, msg, sizeof(int));
	if (r != sizeof(int)) {
	    printf("test10: read returned %d\n", r);
	    return;
	}

	r  = read(s1, msg, 2 * sizeof(int));
	if (r != 2 * sizeof(int)) {
	    printf("test10: read s1 returned %d\n", r);
	    return;
	}
    }

    if (close(s1) < 0) {
	printf("test: close failed\n");
    }

}


void 
test(int test, int size)
{
    int s;
    int len;
    int on;
    int n = htonl(test);
    int sz = htonl(size);
    int m;

    printf("run test %d size %d\n", test, size);

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	perror("socket");
	exit(1);
    }
    
    on = 1;
    if (setsockopt(s,  SOL_SOCKET, SO_DEBUG, &on, sizeof(on)) < 0) {
	perror("setsockopt");
	exit(1);
    }

#if 0
    if (setsockopt(s,  SOL_SOCKET, SO_LINGER, &on, sizeof(on)) < 0) {
	perror("setsockopt");
	exit(1);
    }
#endif

    len = sizeof(to);

    if (connect(s, (struct sockaddr *) &to, len) < 0) {
	perror("connect");
	exit(1);
    }


    memcpy(msg, &n, sizeof(int));
    memcpy(msg + sizeof(int), &sz, sizeof(int));

    if ((m = write(s, msg, 2*sizeof(n))) != 2*sizeof(n)) {
	printf("test: write returned %d\n", m);
	exit(1);
    }

    switch(test) {
    case 1:
	test1(s);
	break;
    case 2:
	test2(s);
	break;
    case 3:
	test3(s);
	break;
    case 4:
	test4(s, size);
	break;
    case 10:
	test10(s, size);
	break;
    case 20:
	test20(s, size);
	break;
    default:
	printf("test: illegal test %d\n", n);
	break;
    }
    

    if (close(s) < 0) {
	printf("test: close failed\n");
    }

    printf("run test %d done\n", test);
}


int
main(int argc, char *argv[])
{
    char server_can_host_name[64];
    char *server_name;
    int port;
    int i;

    progname = argv[0];
    if (argc < 3) {
	printf("usage: %s server port\n", progname);
	exit(1);
    }
    
    server_name = argv[1];
    port = atoi(argv[2]);

    to.sin_family = AF_INET;
    to.sin_port = htons(port);
    if ((to.sin_addr.s_addr = resolve_name(server_name,
                                               server_can_host_name, 64)) == 0) {
        printf("%s: unknown host %s\n", progname, server_name);
        exit(1);
    }

    printf("%s: server %s port %d\n", progname, server_name, to.sin_port);

    for (i = 0; i < NTEST; i++) {
	test(10, 1000);
	test(3, 4096);
	test(4, 1000 * 1024);
    }
}



