
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
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netinet/in.h>

extern char *get_ip_from_name(const char *name);


int main (int argc, char *argv[])
{
   char buf[8193];
   int ret;
   char req[512];
   int portno;
   int fd;
   struct sockaddr_in webaddr;
   char *ip_addr;

   if ((argc != 3) && (argc != 4)) {
      fprintf (stderr, "Usage: %s <server> <docname> [ <portno> ]\n", argv[0]);
      exit(0);
   }

   if (argc == 4) {
      portno = atoi(argv[3]);
   } else {
      portno = 80;
   }

   ip_addr = get_ip_from_name(argv[1]);
   if (ip_addr == NULL) {
      fprintf (stderr, "server %s is unknown (i.e., not found in hosttable)\n", argv[1]);
      exit(0);
   }

   if ((fd = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
      fprintf (stderr, "Unable to create the main server TCP socket\n");
      exit(0);
   }

   bzero ((char *) &webaddr, sizeof(webaddr));
   webaddr.sin_family = AF_INET;
   webaddr.sin_addr.s_addr = *((uint *) ip_addr);
   webaddr.sin_port = htons (portno);

   if ((ret = connect (fd, (struct sockaddr *) &webaddr, sizeof(webaddr))) == -1) {
      perror ("Failed to connect");
      exit(0);
   }

   //printf ("Connected to server\n");

/*
   sprintf (req, "GET /u1/ganger/public_html/index.html HTTP/1.0\n");
   sprintf (req, "GET /~ganger/ HTTP/1.0\r\n\n");
   sprintf (req, "GET /~josh/blue/ HTTP/1.0\r\n\r\n");
   sprintf (req, "GEAT /~ganger/bob.tcl HTTP/1.0\r\n\n");
   sprintf (req, "GET /~ganger/ HTTP/1.0\n\n");
*/
   sprintf (req, "GET %s HTTP/1.0\r\n\r\n", argv[2]);

   ret = write (fd, req, (strlen(req)+1));
   //printf ("\nret %d\n", ret);
   while (read (fd, buf, 1)) {
      printf("%c", buf[0]);
   }
//printf ("\nDone\n");

   return 1;
}

