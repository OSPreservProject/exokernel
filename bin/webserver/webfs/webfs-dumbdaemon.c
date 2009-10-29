
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


#include "alfs/alfs.h"
#include "webfs.h"

#include <assert.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>

char buf[4096];

#define WEBFS_PORTNO	1081


/*
 * Compute Internet Checksum for "count" bytes beginning at location "addr".
 * Taken from RFC 1071.
 */

static long inet_sum(unsigned short *addr, int count, long start, int last)
{
    register long sum = start;

    /* An unrolled loop */
    while ( count > 15 ) {
       sum += htons (* (unsigned short *) &addr[0]);
       sum += htons (* (unsigned short *) &addr[1]);
       sum += htons (* (unsigned short *) &addr[2]);
       sum += htons (* (unsigned short *) &addr[3]);
       sum += htons (* (unsigned short *) &addr[4]);
       sum += htons (* (unsigned short *) &addr[5]);
       sum += htons (* (unsigned short *) &addr[6]);
       sum += htons (* (unsigned short *) &addr[7]);
       addr += 8;
       count -= 16;
    }

    /*  This is the inner loop */
    while( count > 1 )  {
        sum += htons (* (unsigned short *) addr++);
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


void printRet (ret)
int ret;
{
     if (ret >= 0) {
	  printf ("OK\n");
	  return;
     }

     switch (ret) {
	case ALFS_ELENGTH: printf ("ALFS_ELENGTH\n"); break;
	case ALFS_ENOENT: printf ("ALFS_ENOENT\n"); break;
	case ALFS_EACCESS: printf ("ALFS_EACCESS\n"); break;
	case ALFS_EPERM: printf ("ALFS_EPERM\n"); break;
	case ALFS_EINUSE: printf ("ALFS_EINUSE\n"); break;
	case ALFS_ENFILE: printf ("ALFS_ENFILE\n"); break;
	case ALFS_EISDIR: printf ("ALFS_EISDIR\n"); break;
	case ALFS_EBADF: printf ("ALFS_EBADF\n"); break;
	case ALFS_EINVAL: printf ("ALFS_EINVAL\n"); break;
	case ALFS_EEXIST: printf ("ALFS_EEXIST\n"); break;
	case ALFS_ENOTEMPTY: printf ("ALFS_ENOTEMPTY\n"); break;
	default: printf ("unknown error return val: %d\n", ret); break;
     }
}

int crap_now = 0;
int crap_len;

int main (int argc, char **argv) {
   int ret = 0;
   int len;
   int msg;
   int pathnamelen;
   int filefd = -1;
   int shutdown = 0;
   int sum;
   int sentsum;
   char crapbuf[80];
   int mainfd, fd;
   struct sockaddr_in webfsaddr;

   memset (crapbuf, -1, 80);
   if (argc != 2) {
      printf ("Usage: %s <devno>\n", argv[0]);
      exit (0);
   }

   //webfs_initFS (argv[1], 536870912);
   webfs_mountFS (argv[1]);

   printf("WebFS initialized successfully\n");

   if ((mainfd = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
      fprintf (stderr, "Unable to create the main server TCP socket\n");
      exit(0);
   }

   bzero ((char *) &webfsaddr, sizeof(webfsaddr));
   webfsaddr.sin_family = AF_INET;
   webfsaddr.sin_addr.s_addr = htonl (INADDR_ANY);
   webfsaddr.sin_port = htons (WEBFS_PORTNO);

   if (bind (mainfd, (struct sockaddr *) &webfsaddr, sizeof (webfsaddr)) < 0) {
      perror ("Unable to bind to main server socket descriptor");
      exit(0);
   }

   listen (mainfd, 20);

   while (shutdown == 0) {
      if ((fd = accept(mainfd, NULL, NULL)) == -1) {
         perror("accept failed");
         exit (1);
      }

      if ((len = read (fd, buf, 20)) != 20) {
         printf ("read of message failed: %d\n", len);
         exit (1);
      }

      if (sscanf (buf, "%d %d %x", &msg, &pathnamelen, &sentsum) != 3) {
         printf ("ill-formed message from client\n");
         exit (1);
      }
/*
buf[11] = (char) 0;
printf ("%s\n", buf);
*/

      if (msg == 0) {
         printf ("client said hello\n");
         ret = 1;
      } else if (msg == 3) {
         printf ("client said shutdown\n");
         ret = 1;
         shutdown = 1;
      } else if (msg == 1) {
         if ((len = read (fd, buf, (pathnamelen+1))) != (pathnamelen+1)) {
            printf ("read of mkdir pathname failed: %d (expected %5d)\n", len, (pathnamelen+1));
            exit (1);
         }
         printf ("mkdir: %s\n", buf);
         ret = webfs_mkdir (buf, 0777);
         if (ret == 0) {
            ret = 1;
         } else if (ret != ALFS_EEXIST) {
            printf ("failed to mdir %s (ret %d)\n", buf, ret);
         }
      } else if (msg == 2) {
         if ((len = read (fd, buf, (pathnamelen+1))) != (pathnamelen+1)) {
            printf ("read of write pathname failed: %d (expected %5d)\n", len, (pathnamelen+1));
            exit (1);
         }
         printf ("file: %s\n", buf);
/*
if (strcmp (buf, "/home/am1/ganger/public_html/papers/sigmetrics93.ps.Z") == 0) {
   crap_now = 1;
}
*/
         filefd = webfs_open (buf, OPT_RDWR | OPT_CREAT, 0777);
         if (filefd >= 0) {
            ret = 1;
         }
      } else if (msg == 4) {
         if ((len = read (fd, buf, (pathnamelen+1))) != (pathnamelen+1)) {
            printf ("read of mkdir pathname failed: %d (expected %5d)\n", len, (pathnamelen+1));
            exit (1);
         }
         printf ("get file: %s\n", buf);
         filefd = webfs_open (buf, OPT_RDONLY, 0777);
         ret = filefd;
         if (filefd >= 0) {
            ret = 1;
         } else {
            printf ("unable to open file: %d\n", ret);
            printRet(ret);
         }
      } else {
         printf ("Unknown message received: %d\n", msg);
         exit (1);
      }

      sprintf (buf, "%3d", ret);
      if ((len = write (fd, buf, 4)) != 4) {
         printf ("write of response failed: %d\n", len);
         exit (1);
      }

      if (msg == 4) {
         crap_len = 0;
         while ((len = webfs_read (filefd, buf, 4096)) > 0) {
            crap_len += len;
            ret = write (fd, buf, len);
            if (ret != len) {
               printf ("failed to write correct amount of data: %d != %d\n", ret, len);
            }
         }
         printf ("done sending: len %d\n", crap_len);
         ret = webfs_close(filefd);
      }

      if (msg == 2) {
         crap_len = 0;
         sum = 0;
         while ((len = read (fd, buf, 4096)) > 0) {
            crap_len += len;
            sum = inet_sum ((unsigned short *) buf, len, sum, 0);
/*
if (crap_now) {
   printf ("running sum: %x (count %d)\n", sum, crap_len);
}
*/
            ret = webfs_write(filefd, buf, len);
            if (ret != len) {
               printf ("failed to write correct amount of data: %d != %d\n", ret, len);
               if (ret < 0) {
                  printRet (ret);
               }
               exit (1);
            }
memset (buf, -1, 4096);
         }
         if (len < 0) {
            printf ("Error on file read\n");
            exit (1);
         }
         printf ("length: %d (sum %x)\n", crap_len, sum);
         ret = webfs_close(filefd);
         if (ret != 0) {
            printf ("Error closing file:");
            printRet (ret);
         }
         if (sentsum != sum) {
            printf ("sums don't match: sentsum %x, recvsum %x\n", sentsum, sum);
            crap_now = 1;
         }
      }

      close (fd);
if (crap_now) {
   break;
}
   }
/*
   close (mainfd);
*/
   webfs_unmountFS ();

   printf ("done\n");
   //tcp_statistics ();

   exit (0);
}
