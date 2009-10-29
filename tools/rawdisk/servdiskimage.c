
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
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <netdb.h>
#include <sys/fcntl.h>
/*
#include <sys/fcntlcom.h>
*/
#include "assert.h"

#define PORT_MASK	0x00001000

static int fd;
static struct sockaddr_in ServerAddr;
static int ServerAddrLen;
static struct sockaddr_in clientAddr;
static int clientAddrLen;

char buf[65536];


/*
 * Compute Internet Checksum for "count" bytes beginning at location "addr".
 * Taken from RFC 1071.
 */

static long inet_sum(addr, count, start, last)
unsigned short *addr;
int count;
long start;
int last;
{
    register long sum = start;

    /* An unrolled loop */
    while ( count > 15 ) {
       sum += * (unsigned short *) &addr[0];
       sum += * (unsigned short *) &addr[1];
       sum += * (unsigned short *) &addr[2];
       sum += * (unsigned short *) &addr[3];
       sum += * (unsigned short *) &addr[4];
       sum += * (unsigned short *) &addr[5];
       sum += * (unsigned short *) &addr[6];
       sum += * (unsigned short *) &addr[7];
       addr += 8;
       count -= 16;
    }

    /*  This is the inner loop */
    while( count > 1 )  {
        sum += * (unsigned short *) addr++;
        count -= 2;
    }

    /*  Add left-over byte, if any */
    if(count > 0)
        sum += * (unsigned char *) addr;

    if (last) {
       /*  Fold 32-bit sum to 16 bits */
       while (sum>>16) {
          sum = (sum & 0xffff) + (sum >> 16);
       }
       return (~sum & 0xffff);
    }
    return sum;
}


int main (argc, argv)
int argc;
char **argv;
{
   int ret, ret2;
   int filefd;
   int newfd;
   unsigned int pid = getpid ();
   unsigned int portno = pid | PORT_MASK;
   int filesize = 0;
   int sum = 0;
   int bytes;

   if (argc != 2) {
      fprintf (stderr, "Usage: %s <filename>\n", argv[0]);
      exit (0);
   }

   if (strcmp (argv[1], "stdin") == 0) {
      filefd = 0;
   } else if ((filefd = open (argv[1], O_RDONLY)) < 0) {
      fprintf (stderr, "Unable to open file: %s\n", argv[1]);
      exit (0);
   }

   if ((fd = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
      fprintf(stderr, "Couldn't create a TCP socket\n");
      exit(0);
   }

   bzero ((char *) &ServerAddr, sizeof(ServerAddr));
   ServerAddr.sin_family = AF_INET;
   ServerAddr.sin_addr.s_addr = htonl (INADDR_ANY);
   ServerAddr.sin_port = htons (portno);
   ServerAddrLen = sizeof (ServerAddr);

   if (bind (fd, (struct sockaddr *)&ServerAddr, ServerAddrLen) < 0) {
      fprintf(stderr, "Couldn't bind to socket descriptor: portno %d\n", portno);
      exit(0);
   }

   listen (fd, 1);

   printf ("serving file from port %d\n", portno);

   clientAddrLen = sizeof (clientAddr);
   if ((newfd = accept (fd, (struct sockaddr *)&clientAddr, &clientAddrLen)) == -1) {
      perror ("Failed to accept new connection");
      exit(0);
   }

   bytes = 0;
   while ((ret = read (filefd, &buf[0], 4096)) > 0) {
      filesize += ret;
      bytes += ret;
      sum = inet_sum ((unsigned short *) buf, ret, sum, 0);
      ret2 = write (newfd, buf, ret);
      if (ret2 != ret) {
         fprintf (stderr, "Non-matched amount of data sent: %d != %d\n", ret2, ret);
         exit (0);
      }
      if (bytes >= 4194304) {
         printf ("bytes: %7d,   (sum %x)\n", bytes, sum);
         bytes = 0;
      }
   }

   if (ret < 0) {
      perror ("file read failed");
   }

   close (filefd);
   close (fd);

   printf ("Done: %d (sum %x)\n", filesize, sum);

   return 0;
}
