
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

#define  MAX_MSG	(32 * 1024)

/*
 * A hacked up server to test exokernel tcp code.
 */

static char 			*progname;
static char 			msg[MAX_MSG];
static struct sockaddr_in 	me_in;

/* Test reading */
static void 
test1(int s)
{
    int len;

    while (1) {
	if ((len = read(s, msg, MAX_MSG)) < 0) {
	    perror("read");
	    exit(1);
	}

	if (len > 0) printf("cserver: %s\n", msg);
	else {
	    return;
	}
    }
}


/* Test writing */
static void 
test2(int s)
{
    int len;
    int i;

    memcpy(msg, "  hello world", strlen("  hello world") + 1);

    for (i = 0; i < 2; i++) {
	msg[0] = '0' + i % 10;
	
	printf("test2: %d bytes\n", strlen(msg) + 1);

	if ((len = write(s, msg, strlen(msg) + 1)) < 0) {
	    perror("write");
	    exit(1);
	}
    }
}


/* Test large writing */
static void 
test3(int s, int len, int sz)
{
    int i;

    do {
       if (sz != write(s, msg, sz)) {
	   perror("write");
	   exit(1);
       }
       len -= sz;
   } while (len > 0);
}




/* Test large reading */
static void 
test4(int s)
{
    int len;
    int total;

    total = 0;
    while (1) {
	if ((len = read(s, msg, MAX_MSG)) < 0) {
	    perror("read");
	    exit(1);
	}

	if (len <= 0) break;
	
	total += len;
    }
}



/* Test latency */
static void
test10(int s)
{
    int n;
    int len;

    n = sizeof(int);

    if ((len = write(s, msg, n)) < 0) {
	printf("test10: tcp_write failed %d\n", len);
    }

    while (1) {
	if ((len = read(s, msg, n)) < 0) {
	    printf("test10: tcp_read failed %d\n", len);
	}

	if (len <= 0) break;

	if ((len = write(s, msg, n)) < 0) {
	    printf("test10: tcp_write failed %d\n", len);
	}
    }
}



/* Test starts with a 1 byte opcode telling which test to run */
static void 
server(int s)
{
    int n;
    int len;
    int sz;

    if ((len = read(s, &n, sizeof(int))) < 0) {
       perror("read");
       exit(1);
    }
    
    if ((len = read(s, &sz, sizeof(int))) < 0) {
       perror("read");
       exit(1);
    }

    n = ntohl(n);
    sz = ntohl(sz);
    
    printf("test %d sz %d\n", n, sz);

    switch(n) {
    case 1:
	test1(s);
	break;
    case 2:
	test2(s);
	break;
    case 3:
	test3(s, 1000 * 1024, sz);
	break;
    case 4:
	test4(s);
	break;
    case 5:
	test4(s);
	break;
    case 6:
	test3(s, 1000 * 1024, sz);
	break;
    case 7:
	test3(s, 1000 * 1024, sz);
	break;
    case 8:
	close(s);
	test4(s);
	printf("done\n");
	return;
    case 9:
	test4(s);
	break;
    case 10:
	test10(s);
	break;
    default:
	printf("illegal opcode %d\n", n);
	exit(0);
    }
    printf("done\n");
    close(s);
}


int
main(int argc, char *argv[])
{
    int s;
    int on;
    struct sockaddr_in from;
    int len;
    int port;
    int fromlen;
    int news;
    int pid;

    progname = argv[0];
    if (argc < 2) {
	printf("usage: %s port (zero for arbitrary port)\n", progname);
	exit(1);
    }
    
    port = atoi(argv[1]);

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
    if (setsockopt (s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on)) < 0) {
	perror("setsockopt");
	exit(1);
    }


    me_in.sin_family = AF_INET;
    me_in.sin_addr.s_addr = htonl(INADDR_ANY);
    me_in.sin_port = htons(port);
    
    if (bind(s, (struct sockaddr *) &me_in, sizeof(me_in)) < 0) {
	perror("bind");
	exit(1);
    }

    len = sizeof(me_in);
    if (getsockname(s, (struct sockaddr *) &me_in, &len) < 0) {
	perror("getsockname");
	exit(1);
    }

    printf("%s: port %d\n", progname, me_in.sin_port);

    if (listen(s, 5) < 0) {
	perror("listen");
	exit(1);
    }

    for (;;) {
	fromlen = sizeof(from);
	if ((news = accept(s, (struct sockaddr *) &from, &fromlen)) < 0) {
	    perror("accept");
	    exit(1);
	}

	if((pid = fork()) < 0) {
	    perror("fork");
	    exit(1);
	} else if (pid == 0) {	/* child */
	    close(s);
	    server(news);
	    exit(0);
	} else {
	    close(news);
	    waitpid(-1, 0, WNOHANG);
	}
    }
    close(s);
}
