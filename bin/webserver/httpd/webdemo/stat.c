
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <assert.h>

#include "stat.h"

static int portno =0;

static struct sockaddr_in remoteSock;
static int remoteFd;
static int myGroup;

int init_stats (char *server, int port, int group) {
     struct hostent *remote;
     struct sockaddr_in localSock;


     remoteFd = socket (AF_INET, SOCK_DGRAM, 0);
     assert (remoteFd > 0);
     remote = gethostbyname (server);
     remoteSock.sin_family = AF_INET;
     memcpy (&remoteSock.sin_addr.s_addr, remote->h_addr, remote->h_length);
     remoteSock.sin_port = htons (port);

     localSock.sin_family = AF_INET;
     localSock.sin_addr.s_addr = htonl (0);
     localSock.sin_port = htons ((((getpid() << 1) & 0xFE)) + portno);
     portno++;

     if (bind (remoteFd, &localSock, sizeof (localSock)) < 0) {
        printf ("unable to bind to port %d\n", localSock.sin_port);
        perror ("error");
        assert (0);
     }

     myGroup = group;

     return (1);
}

int init_graphs (int numGraphs, char *titles, char *legends, int maxRange, 
		 char *height, char *width) {
     char command[128];
     sprintf (command, "foo_%d_%s_%s_%d_%s_%s_foo_10\n",
	      numGraphs, titles, legends, maxRange, height, width);
     return (sendto (remoteFd, command, strlen (command), 0, &remoteSock,
		     sizeof (remoteSock)));
}

int stats_enable (int which) {
     char command[64];

     sprintf (command, "enable %d", which);
     return (sendto (remoteFd, command, strlen (command), 0, &remoteSock, sizeof (remoteSock))); 
}     

int stats_barrier (char *barrierstr) {
     int ret;
     int dummy;

     ret = sendto (remoteFd, barrierstr, strlen(barrierstr), 0, &remoteSock, sizeof (remoteSock));
     assert (ret == strlen(barrierstr));
     ret = recvfrom (remoteFd, &dummy, 1, 0, 0, &dummy);
     assert (ret == 1);
}

int stats_done (int which) {
     char command[64];

     sprintf (command, "done %d %d", myGroup, which);
     return (sendto (remoteFd, command, strlen (command), 0, &remoteSock, sizeof (remoteSock))); 
}

