
#include <xok/sys_ucall.h>
#include <sys/types.h>
#include <assert.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <exos/tick.h>

#include <xok/disk.h>
#include <xok/buf.h>

#include <xok/pctr.h>

#include <exos/netinet/cksum.h>

int getpid();
#define disk_poll	getpid

char buf[524288];
#define REPEATS 16
#define REPEATS2 16384

/* XXX XXX XXX XXX */
#define ip_sum2 ip_sum

int main (int argc, char *argv[])
{
   struct buf theBuf;
   int rate = ae_getrate ();
   int ticks;
   int time1;
   int lasttime1;
   pctrval v1;                                             
   pctrval v2;
   pctrval v;
   int diskno = -1;
   long sum1 = 0;
   long sum2 = 0;
   long sum3 = 0;
   long sum4 = 0;
   int incr;
   int i;
   int ints;
   int kbpms;

   bzero (&buf[0], 524288);

   if (argc != 2) {
      printf ("Usage: %s <diskno>\n", argv[0]);
      return (1);
   }
   diskno = atoi(argv[1]);

   theBuf.b_dev = diskno;
   theBuf.b_memaddr = &buf[0];
   theBuf.b_resptr = &theBuf.b_resid;
   theBuf.b_flags = B_READ;
   theBuf.b_bcount = 524288;
   theBuf.b_blkno = 0;
   theBuf.b_resid = 1;

   time1 = ae_gettick();
   v1 = rdtsc();
   v2 = rdtsc();
   v = rdtsc();
   lasttime1 = time1;

   if ( sys_disk_request (diskno, 0, 0, &theBuf) != 0) {
      printf ("unable to start request\n");
      return 1;
   }
   while ((disk_poll () > 0) && (theBuf.b_resid != 0)) {
   }

   incr = 8;
   while (incr < 8192) {
      incr *= 2;

      sum1 = 0;
      time1 = ae_gettick();
      for (i=0; i<REPEATS; i++) {
         ints = 0;
         while ((ints + incr) <= 524288) {
            sum1 = inet_cksum ((uint16 *) &buf[ints], incr, sum1);
            ints += incr;
         }
      }
      ticks = ae_gettick() - time1;
      kbpms = (((100*ints) / (ticks * rate / 1000))) * REPEATS;
      printf ("inet_cksum size:\t%7d, bytes/sec %6d, sum %x, ticks %d\n", incr, kbpms, (unsigned) sum1, ticks);

      sum2 = 0;
      time1 = ae_gettick();
      for (i=0; i<REPEATS; i++) {
         ints = 0;
         while ((ints + incr) <= 524288) {
            sum2 = inet_checksum ((uint16 *) &buf[ints], incr, sum2, 1);
            ints += incr;
         }
      }
      ticks = ae_gettick() - time1;
      kbpms = (((100*ints) / (ticks * rate / 1000))) * REPEATS;
      printf ("inet_checksum size:\t%7d, bytes/sec %6d, sum %x, ticks %d\n", incr, kbpms, (unsigned) sum2, ticks);

      sum3 = 0;
      time1 = ae_gettick();
      for (i=0; i<REPEATS; i++) {
         ints = 0;
         while ((ints + incr) <= 524288) {
            ip_sum (sum3, &buf[ints], incr);
            sum3 = fold_32bit_sum (sum3);
            ints += incr;
         }
      }
      ticks = ae_gettick() - time1;
      kbpms = (((100*ints) / (ticks * rate / 1000))) * REPEATS;
      printf ("ip_sum size:\t\t%7d, bytes/sec %6d, sum %x, ticks %d\n", incr, kbpms, (unsigned) sum3, ticks);

      sum4 = 0;
      time1 = ae_gettick();
      for (i=0; i<REPEATS; i++) {
         ints = 0;
         while ((ints + incr) <= 524288) {
            ip_sum2 (sum4, &buf[ints], incr);
            sum4 = fold_32bit_sum (sum4);
            ints += incr;
         }
      }
      ticks = ae_gettick() - time1;
      kbpms = (((100*ints) / (ticks * rate / 1000))) * REPEATS;
      printf ("ip_sum2 size:\t\t%7d, bytes/sec %6d, sum %x, ticks %d\n", incr, kbpms, (unsigned) sum4, ticks);
   }

   printf ("\nPHASE II\n");

   incr = 8;
   while (incr < 1024) {
      incr *= 2;

      sum1 = 0;
      time1 = ae_gettick();
      for (i=0; i<REPEATS2; i++) {
         ints = 0;
         while ((ints + incr) <= 1024) {
            sum1 = inet_cksum ((uint16 *) &buf[ints], incr, sum1);
            ints += incr;
         }
      }
      ticks = ae_gettick() - time1;
      kbpms = (((100*ints) / (ticks * rate / 1000))) * REPEATS2;
      printf ("inet_cksum size:\t%7d, bytes/sec %6d, sum %x, ticks %d\n", incr, kbpms, (unsigned) sum1, ticks);

      sum2 = 0;
      time1 = ae_gettick();
      for (i=0; i<REPEATS2; i++) {
         ints = 0;
         while ((ints + incr) <= 1024) {
            sum2 = inet_checksum ((uint16 *) &buf[ints], incr, sum2, 1);
            ints += incr;
         }
      }
      ticks = ae_gettick() - time1;
      kbpms = (((100*ints) / (ticks * rate / 1000))) * REPEATS2;
      printf ("inet_checksum size:\t%7d, bytes/sec %6d, sum %x, ticks %d\n", incr, kbpms, (unsigned) sum2, ticks);

      sum3 = 0;
      time1 = ae_gettick();
      for (i=0; i<REPEATS2; i++) {
         ints = 0;
         while ((ints + incr) <= 1024) {
            ip_sum (sum3, (uint) &buf[ints], incr);
            sum3 = fold_32bit_sum (sum3);
            ints += incr;
         }
      }
      ticks = ae_gettick() - time1;
      kbpms = (((100*ints) / (ticks * rate / 1000))) * REPEATS2;
      printf ("ip_sum size:\t\t%7d, bytes/sec %6d, sum %x, ticks %d\n", incr, kbpms, (unsigned) sum3, ticks);

      sum4 = 0;
      time1 = ae_gettick();
      for (i=0; i<REPEATS2; i++) {
         ints = 0;
         while ((ints + incr) <= 1024) {
            ip_sum2 (sum4, (uint) &buf[ints], incr);
            sum4 = fold_32bit_sum (sum4);
            ints += incr;
         }
      }
      ticks = ae_gettick() - time1;
      kbpms = (((100*ints) / (ticks * rate / 1000))) * REPEATS2;
      printf ("ip_sum2 size:\t\t%7d, bytes/sec %6d, sum %x, ticks %d\n", incr, kbpms, (unsigned) sum4, ticks);
   }

   printf ("Done: sum1 %x, sum2 %x, sum3 %x, sum4 %x\n", (unsigned) sum1, (unsigned) sum2, (unsigned) sum3, (unsigned) sum4);
   return (1);
}

