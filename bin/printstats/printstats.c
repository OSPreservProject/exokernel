
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

#include <xok/sys_ucall.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <xok/sysinfo.h>
#include <fd/cffs/cffs.h>

void standard_stats(void);
void line_stats_header(void);
void line_stats(void);


void usage(void) {
  extern char *__progname;
  fprintf(stderr,"Usage: %s [-l] [-c count] [-w wait]\n",__progname);
  fprintf(stderr,"-l  shows statistics in one line\n");
  fprintf(stderr,"-c  repeat displays count times\n");
  fprintf(stderr,"-w  pauses wait seconds between displays\n");
  fprintf(stderr,"    if a wait is specified, but no count, it repeats forever\n");
  exit (0);
}

int main (int argc, char **argv) {
  extern char *optarg;
  extern int optind;
  int ch, cflag, lflag, wflag, wait, count;
  int i;

  lflag = 0;
  wflag = 0;
  cflag = 0;
  count = 1;			/* default count once */
  wait = 0;

  opterr = 0;
  while ((ch = getopt(argc, argv, "lc:w:")) != -1)
    switch(ch) {
    case 'c':
      count = atoi(optarg);
      if (count < 0) {
	fprintf(stderr,"count must be positive\n");
	usage();
      }
      cflag = 1;
      break;
    case 'w':
      wait = atoi(optarg);
      if (wait < 0) {
	fprintf(stderr,"wait seconds must be positive\n");
	usage();
      }
      wflag = 1;
      break;
    case 'l':
      lflag = 1;
      break;
    case '?':
    default:
      usage();
    }
  argc -= optind;
  argv += optind;

  if (!cflag && wflag) count = 2000000000;
  for(i = 0; i < count;  i++) {
    if (lflag) {
      if ((i % 22) == 0) line_stats_header();
      line_stats();
    } else  standard_stats();
    if (wflag) sleep(wait);
  }
  
  return (0);
}

void standard_stats(void) {

   int j,k;
   u_int i;
   unsigned long long total = 0, secwrites = 0;
   unsigned long long reads = 0, secreads = 0;
   unsigned long long queuedReads = 0;
   unsigned long long queuedWrites = 0;
   unsigned long long ongoingReads = 0;
   unsigned long long ongoingWrites = 0;

   printf ("System-wide statistics\n");
   printf ("======================\n\n");

   printf ("System calls: %d\n", sys_get_nsyscalls ());
   printf ("Physical pages: %d (free pages %d, percentage %d)\n", __sysinfo.si_nppages, __sysinfo.si_nfreepages, (__sysinfo.si_nfreepages * 100 / __sysinfo.si_nppages));
   printf ("Valid, but free, bc entries: %d\n", __sysinfo.si_nfreebufpages);
   printf ("Dirty bc entries: %d\n", __sysinfo.si_ndpages);
   printf ("XN registry entries: %d\n", __sysinfo.si_xn_entries);
   printf ("Kernel-pinned user (or bc) pages: %d\n", __sysinfo.si_pinnedpages);
   printf ("\n");

   for (i=0; i < __sysinfo.si_ndisks; i++) {
      u_int reqs = __sysinfo.si_disks[i].d_readcnt + __sysinfo.si_disks[i].d_writecnt;
      total += reqs;
      reads += __sysinfo.si_disks[i].d_readcnt;
      secwrites += __sysinfo.si_disks[i].d_writesectors;
      secreads += __sysinfo.si_disks[i].d_readsectors;
      ongoingReads += __sysinfo.si_disks[i].d_ongoingReads;
      ongoingWrites += __sysinfo.si_disks[i].d_ongoingWrites;
      queuedReads += __sysinfo.si_disks[i].d_queuedReads;
      queuedWrites += __sysinfo.si_disks[i].d_queuedWrites;
      printf ("Disk %u\tstart: %qu size: %qu requests initiated: %u\n",
	      i,
	      __sysinfo.si_disks[i].d_part_off,
	      __sysinfo.si_disks[i].d_size,
	      reqs);
      printf ("\treads %u, writes %u, interrupts %u)\n", 
	      __sysinfo.si_disks[i].d_readcnt, 
	      __sysinfo.si_disks[i].d_writecnt,
	      __sysinfo.si_disks[i].d_intrcnt);
      printf ("\tsectors read: %qu sectors written: %qu)\n",
	      __sysinfo.si_disks[i].d_readsectors,
	      __sysinfo.si_disks[i].d_writesectors);
      if (i == __sysinfo.si_ndisks-1)
	printf("\n");
   }
   printf ("Total across disks, %qd requests (reads %qd, writes %qd, interrupts %qd)\n", total, reads, (total-reads), __sysinfo.si_diskintrs);
   printf ("Total across disks, %qd sectors (reads %qd, writes %qd)\n", secreads + secwrites, secreads, secwrites);
   printf ("Ongoing reads %qd, ongoing writes %qd, ongoing requests %qd\n", ongoingReads, ongoingWrites, (ongoingReads + ongoingWrites));
   printf ("Queued reads %qd, queued writes %qd, queued requests %qd\n", queuedReads, queuedWrites, (queuedReads + queuedWrites));
   printf ("\n");

   printf ("Network interface stats:\n");
   for (i=0; i<__sysinfo.si_nnetworks; i++) {
      struct network *xoknet = &__sysinfo.si_networks[i];
      printf ("%s:  %qd xmits and %qd rcvs (%qd discarded, %d+%d I/O errs)\n", xoknet->cardname, xoknet->xmits, xoknet->rcvs, xoknet->discards, xoknet->inerrs, xoknet->outerrs);
      printf ("           %qd interrupts (%qd for rcvs, %qd for xmits, %qd other)\n", xoknet->intrs, xoknet->rxintrs, xoknet->txintrs, xoknet->otherintrs);
   }
   printf ("\n");

   j = 0;
   for (i=0; i<MAX_DISKS; i++) {
      if (__sysinfo.si_xn_blocks[i]) {
         printf ("XN blocks for disk #%d: %d\n", i, __sysinfo.si_xn_blocks[i]);
         printf ("unallocated XN blocks for disk #%d: %d (%02d percent)\n", i, __sysinfo.si_xn_freeblks[i], (u_int)((quad_t)100 * __sysinfo.si_xn_freeblks[i] / __sysinfo.si_xn_blocks[i]));
	 j = 1;
      }
      if (i == MAX_DISKS-1 && j == 1) printf("\n");
   }
   cffs_printstats ();

   j = sys_get_num_cpus();
   printf ("SMP information: %d CPUs\n",j);
   for(i = 0; i < j; i++) {
     printf("cpu %d: total ticks %qd, idle ticks %qd ", i, 
            __sysinfo.si_percpu_ticks[i], 
	    __sysinfo.si_percpu_idle_ticks[i]);
     if (__sysinfo.si_percpu_ticks[i] != 0) 
       printf(" percentage %qd",
	      (__sysinfo.si_percpu_idle_ticks[i]*100/
	       __sysinfo.si_percpu_ticks[i]));
     printf("\n");
   }

   printf ("\n");

   printf ("Interrupt statistics:\n");
   for(i = 0; i < MAX_IRQS; i++) {
     for(k = 0; k < j; k++)
     {
	if (__sysinfo.si_intr_stats[i][k] != 0) break;
     }
     if (k == j) continue;
     printf("IRQ %2d:  ",i);
     for(k = 0; k < j; k++) {
       if ((k % 2)==0 && k != 0) 
         printf("         ");
       printf("cpu %2d: %16qd  ",k,__sysinfo.si_intr_stats[i][k]);
       if ((k % 2)==1) {
         printf("\n");
       }
     }
     if ((k % 2)==1) printf("\n");
   }
   sys_null();
}


void line_stats_header(void) {
  int i;
  printf("idle ");
  printf("%-6s %-3s ","freep","fl");
  for (i=0; i<__sysinfo.si_nnetworks; i++)
    printf("%-3s %-8s %-8s %-8s ","if","trans","recv","discard");
  printf("\n");
}

void line_stats(void) {
  int i;
  /* idle time */
  printf("%2d%%  ",(int)(__sysinfo.si_percpu_idle_ticks[0] * 100 / __sysinfo.si_system_ticks));
  /* free pages, and percentage */
  printf("%-6d %2d%% ",__sysinfo.si_nfreepages, (__sysinfo.si_nfreepages * 100 / __sysinfo.si_nppages));
  for (i=0; i<__sysinfo.si_nnetworks; i++) {
    struct network *xoknet = &__sysinfo.si_networks[i];
    printf ("%s %-8qd %-8qd %-8qd ", xoknet->cardname, xoknet->xmits, xoknet->rcvs, xoknet->discards);
  }
  printf("\n");
}
