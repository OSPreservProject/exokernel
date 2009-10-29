
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
#include <net/tcp/libtcp.h>
#include <assert.h>

#include <netinet/ip.h>
#include <exos/netinet/fast_eth.h>

#include <exos/netinet/hosttable.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <exos/tick.h>

#include <xok/disk.h>
#include <xok/sysinfo.h>

/*
#define NODISK
*/


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


char buf[131072];

int main (int argc, char *argv[])
{
   void *handle;
   uint32 ip_src;
   uint32 ip_dst;
   int ret = 0;
   struct buf theBuf[2];
   int pingpong = 0;
   int serverport;
   int iters;
   int rate = ae_getrate ();
   int ticks;
   int time1;
   int bpms;
   int bytes = 0;
   int lasttime1;
   int diskno = -1;
   int disksize = -1;
   int sum = 0;

   bzero (&buf[0], 131072);

printf ("NO GUARANTEES: THIS HAS NOT BEEN TESTED SINCE THE LAST DISK INTERFACE CHANGE\n");

   if (argc != 4) {
      printf ("Usage: %s <server> <serverport> <diskno>\n", argv[0]);
      exit (1);
   }
   serverport = atoi(argv[2]);
   diskno = atoi(argv[3]);
   assert ((diskno >= 0) && (diskno < __sysinfo.si_ndisks));
   printf ("trying to send image of disk #%d to port %d on %s\n", diskno, serverport, argv[1]);

   theBuf[0].b_dev = diskno;
   theBuf[0].b_memaddr = &buf[0];
   theBuf[0].b_resptr = &theBuf[0].b_resid;
   theBuf[0].b_flags = B_READ;
   theBuf[0].b_bcount = 512;

#ifndef NODISK
   disksize = __sysinfo.si_disks[diskno].d_size;
   assert (disksize > 0);
#else
   disksize = 33554432 / 512;
#endif

   theBuf[0].b_resid = -1;
   theBuf[0].b_blkno = 0;

   theBuf[1].b_dev = diskno;
   theBuf[1].b_blkno = 0;
   theBuf[1].b_memaddr = &buf[65536];
   theBuf[1].b_resid = -1;
   theBuf[1].b_resptr = &theBuf[1].b_resid;
   theBuf[1].b_flags = B_READ;
   theBuf[1].b_bcount = 0;

{unsigned char *ipstr = get_ip_from_name(argv[1]);
unsigned char *ethstr = get_ether_from_ip(ipstr, 0);
printf ("%s is at %d.%d.%d.%d with ethernet addr %x %x %x %x %x %x\n", argv[1], ipstr[0], ipstr[1], ipstr[2], ipstr[3], ethstr[0], ethstr[1], ethstr[2], ethstr[3], ethstr[4], ethstr[5]);
}

   memcpy (&ip_src, ip_my_addr, 4);
   memcpy (&ip_dst, get_ip_from_name(argv[1]), 4);
   if ((handle = (void *) tcp_connect (serverport, ip_dst, get_ether_from_ip(get_ip_from_name(argv[1]), 0), 0, ip_src, my_eth_addr)) == 0) {
      printf ("cannot establish connection to server\n");
      assert (0);
   }

   theBuf[0].b_bcount = 512 * min (disksize, (65536 / 512));
   theBuf[0].b_resid = 0;
#ifndef NODISK
   if ( sys_disk_request (&__sysinfo.si_pxn[diskno], 0, 0, &theBuf[pingpong], CAP_ROOT) != 0) {


      printf ("unable to start request\n");
      assert (0);
   }
#endif
   pingpong = 1;
   theBuf[1].b_blkno = (min(disksize, (65536/512)));
   disksize -= min (disksize, (65536/512));

printf ("Connected to server\n");

   time1 = ae_gettick();
   lasttime1 = time1;

   while (disksize > 0) {

      int bcount = 512 * min (disksize, (65536/512));
      theBuf[pingpong].b_bcount = bcount;
      theBuf[pingpong].b_resid = 0;
#ifndef NODISK
      if ( sys_disk_request (&__sysinfo.si_pxn[diskno], 0, 0, &theBuf[pingpong], CAP_ROOT) != 0) {
         printf ("unable to start request\n");
         assert (0);
      }
#endif
      pingpong = (pingpong) ? 0 : 1;
      disksize -= bcount / 512;
      while ((theBuf[pingpong].b_resid == 0) && (getpid())) ;
      theBuf[pingpong].b_blkno = theBuf[((pingpong) ? 0 : 1)].b_blkno + (theBuf[((pingpong) ? 0 : 1)].b_bcount / 512);

      for (iters=0; iters < ((theBuf[pingpong].b_bcount + 4095)/4096); iters++) {
         if ((ret = tcp_write (handle, &buf[((pingpong*65536)+(iters*4096))], min(4096, (theBuf[pingpong].b_bcount-(iters*4096))))) != min(4096, (theBuf[pingpong].b_bcount-(iters*4096)))) {
            printf ("failed tcp_write (1): ret %d\n", ret);
            exit (1);
         }
      }

      assert ((theBuf[pingpong].b_bcount % 2) == 0);
      if (theBuf[pingpong].b_bcount) {
         sum = inet_sum ((unsigned short *) &buf[(pingpong*65536)], theBuf[pingpong].b_bcount, sum, 0);
      }

      bytes += theBuf[((pingpong) ? 0 : 1)].b_bcount;
      if (bytes >= 4194304) {
         ticks = ae_gettick () - lasttime1;
         bpms = (ticks != 0) ? ((bytes / (ticks * rate / 1000)) * 1000) : 999999;
         printf ("bytes: %7d, bytes/sec %6d (sum %x)\n", bytes, bpms, sum);
         lasttime1 = ae_gettick ();
         bytes = 0;
      }
/*
      if (theBuf[pingpong].b_blkno >= (32 * 1024 * 1024 / 512)) {
         break;
      }
*/
   }

   pingpong = (pingpong) ? 0 : 1;
   while ((theBuf[pingpong].b_resid == 0) && (getpid())) ;
   for (iters=0; iters < ((theBuf[pingpong].b_bcount + 4095)/4096); iters++) {
      if ((ret = tcp_write (handle, &buf[((pingpong*65536)+(iters*4096))], min(4096, (theBuf[pingpong].b_bcount-(iters*4096))))) != min(4096, (theBuf[pingpong].b_bcount-(iters*4096)))) {
         printf ("failed tcp_write (2): ret %d\n", ret);
         exit (1);
      }
   }

   assert ((theBuf[pingpong].b_bcount % 2) == 0);
   sum = inet_sum ((unsigned short *) &buf[(pingpong*65536)], theBuf[pingpong].b_bcount, sum, 0);

   theBuf[pingpong].b_blkno += theBuf[pingpong].b_bcount / 512;

   ticks = ae_gettick () - time1;
   bpms = (ticks != 0) ? (((theBuf[pingpong].b_blkno * 512) / (ticks * rate / 1000)) * 1000) : 9999999;
   printf ("total bytes: %8qu, overall bytes/sec %6d, total seconds %d (sum %x)\n", theBuf[pingpong].b_blkno * 512, bpms, (ticks * rate / 1000000), sum);

   tcp_close (handle);

   tcp_statistics ();

   printf ("Done\n");
   exit (0);
}

