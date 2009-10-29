
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
#include <assert.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

/*
#define ONLY_EMBEDGROUP
*/


#ifdef EXOPC
#include <fd/cffs/cffs.h>
#include <xok/sysinfo.h>
extern int cffs_diskreads, cffs_diskwrites;
#define ae_getrate()	__sysinfo.si_rate
#define ae_gettick()	__sysinfo.si_system_ticks
#else
#include <time.h>
int cffs_diskreads = 0, cffs_diskwrites = 0;
#define ae_getrate() 1000
static inline int ae_gettick() {
  struct timeval t; 
  gettimeofday(&t,(struct timezone *)0);
//  printf("time %ld %ld\n",t.tv_sec,(t.tv_sec - 847910277)*10000 + t.tv_usec/100);
  return (t.tv_sec - 847910277)*(1000000/ae_getrate()) + 
    t.tv_usec/ae_getrate();
}
#endif

#define PRINTFBANNER \
   printf("disk r %d, w %d secs %.2f sync: %.1f, files/sec %.1f \n", 	\
	  (cffs_diskreads - startdiskreads), 				\
	  (cffs_diskwrites - startdiskwrites), 				\
	  tmpval, 							\
	  synctime,  \
	  ((double) NUM_FILES / tmpval))


#define NUM_FILES	1000
#define FILES_PER_DIR	100
#define NUM_DIRS	((NUM_FILES + FILES_PER_DIR - 1) / FILES_PER_DIR)
#define	FILE_SIZE	1024

int phase1ticks = 0;
int phase2ticks = 0;
int phase3ticks = 0;
int phase4ticks = 0;

char buf[FILE_SIZE];


static inline void empty_buffer_cache ()
{
#ifdef EXOPC
#if 0
  long tick;
  double tmpval;
  tick = ae_gettick();
#endif
  cffs_flush ();
#if 0
  tmpval = (double) (ae_gettick() - tick) * (double) ae_getrate() / (double) 1000000.0;
  printf ("cffs_flush time: %.2f \n",tmpval);
#endif
#endif
}

double synctime = 0;
static inline void sync_buffer_cache ()
{
  long tick;
  tick = ae_gettick();
  sync();
  synctime = (double) (ae_gettick() - tick) * (double) ae_getrate() / (double) 1000000.0;
//  printf ("sync time: %.2f \n",tmpval);
}


int main (int argc, char **argv)
{
   int i, j;
   char name[200];
   int starttick;
   double tmpval;
   int ret;
   int fd;
   int startdiskreads;
   int startdiskwrites;
#ifndef ONLY_EMBEDGROUP
#ifdef EXOPC
   int iterno = 1;
#endif
#endif

   if (argc != 1) {
      printf ("Usage: %s\n", argv[0]);
      exit (0);
   }

   assert (NUM_FILES == (FILES_PER_DIR * NUM_DIRS));

#ifdef EXOPC
   cffs_softupdates = 0;
   cffs_embedinodes = 1;
   cffs_usegrouping = 1;
#ifndef ONLY_EMBEDGROUP
main_repeat:
#endif
#endif

   for (i=0; i<NUM_DIRS; i++) {
      sprintf (name, "%d", i);
      ret = mkdir (name, 0777);
      if (ret != 0) {
         printf ("bad return from mkdir (%s): %d\n", name, ret);
      }
      assert (ret == 0);
   }

#ifdef EXOPC
   printf ("Small benchmark starting: (embedinodes %d, usegrouping %d, softupdates %d)\n", cffs_embedinodes, cffs_usegrouping, cffs_softupdates);
#else
   printf ("Small benchmark starting: (on UNIX)\n");
#endif

   /* Phase 1 -- allocate and write */

   empty_buffer_cache ();

   startdiskreads = cffs_diskreads;
   startdiskwrites = cffs_diskwrites;
   starttick = ae_gettick ();
   for (i=0; i<NUM_DIRS; i++) {
      for (j=0; j<FILES_PER_DIR; j++) {
         sprintf (name, "%d/%d", i, j);
         fd = open (name, (O_CREAT|O_RDWR), 0777);
         assert (fd > 0);
         ret = write (fd, buf, FILE_SIZE);
         if (ret != FILE_SIZE) {
            printf ("bad return from write: %d\n", ret);
         }
         assert (ret == FILE_SIZE);
         ret = close (fd);
         assert (ret == 0);
      }
   }

   sync_buffer_cache ();

   phase1ticks = ae_gettick() - starttick;
   assert (phase1ticks != 0);
   tmpval = (double) phase1ticks * (double) ae_getrate() / (double) 1000000.0;
   printf ("Phase 1 (create/write):");
   PRINTFBANNER;

   /* Phase 2 -- read */

   empty_buffer_cache ();

   startdiskreads = cffs_diskreads;
   startdiskwrites = cffs_diskwrites;
   starttick = ae_gettick ();
   for (i=0; i<NUM_DIRS; i++) {
      for (j=0; j<FILES_PER_DIR; j++) {
         sprintf (name, "%d/%d", i, j);
         fd = open (name, (O_RDONLY), 0777);
         if (fd < 0) {
            printf ("bad return from open %s: %d\n", name, fd);
         }
         assert (fd > 0);
         ret = read (fd, buf, FILE_SIZE);
         if (ret != FILE_SIZE) {
            printf ("bad return from read %s: %d\n", name, ret);
         }
         assert (ret == FILE_SIZE);
         ret = close (fd);
         assert (ret == 0);
      }
   }

   sync_buffer_cache ();

   phase2ticks = ae_gettick() - starttick;
   assert (phase2ticks != 0);
   tmpval = (double) phase2ticks * (double) ae_getrate() / (double) 1000000.0;
   printf ("Phase 2 (read):");
   PRINTFBANNER;

   /* Phase 3 -- overwrite */

   empty_buffer_cache ();

   startdiskreads = cffs_diskreads;
   startdiskwrites = cffs_diskwrites;
   starttick = ae_gettick ();
   for (i=0; i<NUM_DIRS; i++) {
      for (j=0; j<FILES_PER_DIR; j++) {
         sprintf (name, "%d/%d", i, j);
         fd = open (name, (O_RDWR), 0777);
         assert (fd > 0);
         ret = write (fd, buf, FILE_SIZE);
         assert (ret == FILE_SIZE);
         ret = close (fd);
         assert (ret == 0);
      }
   }

   sync_buffer_cache ();

   phase3ticks = ae_gettick() - starttick;
   assert (phase3ticks != 0);
   tmpval = (double) phase3ticks * (double) ae_getrate() / (double) 1000000.0;
   printf ("Phase 3 (overwrite):");
   PRINTFBANNER;


   /* Phase 4 -- remove */

   empty_buffer_cache ();

   startdiskreads = cffs_diskreads;
   startdiskwrites = cffs_diskwrites;
   starttick = ae_gettick ();
   for (i=0; i<NUM_DIRS; i++) {
      for (j=0; j<FILES_PER_DIR; j++) {
         sprintf (name, "%d/%d", i, j);
         ret = unlink (name);
         assert (ret == 0);
      }
   }

   sync_buffer_cache ();

   phase4ticks = ae_gettick() - starttick;
   assert (phase4ticks != 0);
   tmpval = (double) phase4ticks * (double) ae_getrate() / (double) 1000000.0;
   printf ("Phase 4 (remove):");
   PRINTFBANNER;

   for (i=0; i<NUM_DIRS; i++) {
      sprintf (name, "%d", i);
      ret = rmdir (name);
      assert (ret == 0);
   }

#ifndef ONLY_EMBEDGROUP
#ifdef EXOPC
   /* what happened to switch? */
   if (iterno == 1) {
      cffs_softupdates = 0;
      cffs_embedinodes = 0;
      cffs_usegrouping = 0;
      iterno++;
      printf ("\n");
      goto main_repeat;
   } else if (iterno == 2) {
      cffs_softupdates = 0;
      cffs_embedinodes = 1;
      cffs_usegrouping = 0;
      iterno++;
      printf ("\n");
      goto main_repeat;
   } else if (iterno == 3) {
      cffs_softupdates = 0;
      cffs_embedinodes = 0;
      cffs_usegrouping = 1;
      iterno++;
      printf ("\n");
      goto main_repeat;
   } else if (iterno == 4) {
      cffs_softupdates = 1;
      cffs_embedinodes = 0;
      cffs_usegrouping = 0;
      iterno++;
      printf ("\n");
      goto main_repeat;
   } else if (iterno == 5) {
      cffs_softupdates = 1;
      cffs_embedinodes = 1;
      cffs_usegrouping = 0;
      iterno++;
      printf ("\n");
      goto main_repeat;
   } else if (iterno == 6) {
      cffs_softupdates = 1;
      cffs_embedinodes = 0;
      cffs_usegrouping = 1;
      iterno++;
      printf ("\n");
      goto main_repeat;
   } else if (iterno == 7) {
      cffs_softupdates = 1;
      cffs_embedinodes = 1;
      cffs_usegrouping = 1;
      iterno++;
      printf ("\n");
      goto main_repeat;
   }
#endif
#endif

   exit (0);
}

