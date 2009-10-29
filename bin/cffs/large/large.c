
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

#define MB		(1024 * 1024)

#define NUM_FILES	1000
#define FILES_PER_DIR	100
#define NUM_DIRS	((NUM_FILES + FILES_PER_DIR - 1) / FILES_PER_DIR)
#define BUF_SIZE	4096
#define	FILE_SIZE	(64 * MB)

int phase1ticks = 0;
int phase2ticks = 0;
int phase3ticks = 0;
int phase4ticks = 0;

char buf[BUF_SIZE];


static inline void empty_buffer_cache ()
{
#ifdef EXOPC
   cffs_flush ();
#endif
}


static inline void sync_buffer_cache ()
{
   sync();
}


int main (int argc, char **argv)
{
   int j;
   char name[200];
   int starttick;
   double tmpval;
   int ret;
   int fd;
   int startdiskreads;
   int startdiskwrites;
#ifdef EXOPC
   int iterno = 1;
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
main_repeat:
#endif

#ifdef EXOPC
   printf ("Large benchmark starting: %d MB file\n(embedinodes %d, usegrouping %d, softupdates %d)\n", (FILE_SIZE/MB), cffs_embedinodes, cffs_usegrouping, cffs_softupdates);
#else
   printf ("Large benchmark starting: %d MB file (on UNIX)\n", (FILE_SIZE/MB));
#endif

   /* Phase 1 -- allocate and write */

   empty_buffer_cache ();

   startdiskreads = cffs_diskreads;
   startdiskwrites = cffs_diskwrites;
   starttick = ae_gettick ();

   sprintf (name, "largefile");
   fd = open (name, (O_CREAT|O_RDWR), 0777);
   assert (fd > 0);
   for (j=0; j<FILE_SIZE; j += BUF_SIZE) {
      ret = write (fd, buf, BUF_SIZE);
      if (ret != BUF_SIZE) {
         printf ("bad return from write: %d\n", ret);
      }
      assert (ret == BUF_SIZE);
   }
   ret = fsync (fd);
   assert (ret == 0);
   ret = close (fd);
   assert (ret == 0);

   phase1ticks = ae_gettick() - starttick;
   assert (phase1ticks != 0);
   tmpval = (double) phase1ticks * (double) ae_getrate() / (double) 1000000.0;
   printf ("Phase 1 (create/write): disk reads %d, writes %d, secs %.2f, MB/sec %.2f\n", (cffs_diskreads - startdiskreads), (cffs_diskwrites - startdiskwrites), tmpval, ((double) FILE_SIZE / (double) MB / tmpval));

   /* Phase 2 -- read */

   empty_buffer_cache ();

   startdiskreads = cffs_diskreads;
   startdiskwrites = cffs_diskwrites;
   starttick = ae_gettick ();

   fd = open (name, (O_RDONLY), 0777);
   if (fd < 0) {
      printf ("bad return from open %s: %d\n", name, fd);
   }
   assert (fd > 0);
   for (j=0; j<FILE_SIZE; j += BUF_SIZE) {
      ret = read (fd, buf, BUF_SIZE);
      assert (ret == BUF_SIZE);
   }
   ret = close (fd);
   assert (ret == 0);

   phase2ticks = ae_gettick() - starttick;
   assert (phase2ticks != 0);
   tmpval = (double) phase2ticks * (double) ae_getrate() / (double) 1000000.0;
   printf ("Phase 2 (read): disk reads %d, writes %d, secs %.2f, MB/sec %.2f\n", (cffs_diskreads - startdiskreads), (cffs_diskwrites - startdiskwrites), tmpval, ((double) FILE_SIZE / (double) MB / tmpval));

   /* Phase 3 -- overwrite */

   empty_buffer_cache ();

   startdiskreads = cffs_diskreads;
   startdiskwrites = cffs_diskwrites;
   starttick = ae_gettick ();

   fd = open (name, (O_RDWR), 0777);
   assert (fd > 0);
   for (j=0; j<FILE_SIZE; j += BUF_SIZE) {
      ret = write (fd, buf, BUF_SIZE);
      assert (ret == BUF_SIZE);
   }
   ret = fsync (fd);
   assert (ret == 0);
   ret = close (fd);
   assert (ret == 0);

   phase3ticks = ae_gettick() - starttick;
   assert (phase3ticks != 0);
   tmpval = (double) phase3ticks * (double) ae_getrate() / (double) 1000000.0;
   printf ("Phase 3 (overwrite): disk reads %d, writes %d, secs %.2f, MB/sec %.2f\n", (cffs_diskreads - startdiskreads), (cffs_diskwrites - startdiskwrites), tmpval, ((double) FILE_SIZE / (double) MB / tmpval));

   ret = unlink (name);
   assert (ret == 0);

#ifdef EXOPC
   if (iterno == 1) {
      cffs_softupdates = 0;
      cffs_embedinodes = 0;
      cffs_usegrouping = 0;
      iterno++;
      printf ("\n");
      goto main_repeat;
   }
#endif

   exit (0);
}

