
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

#include <exos/tick.h>

#include <alfs/alfs.h>

#define NUM_FILES	1000
#define FILES_PER_DIR	100
#define NUM_DIRS	((NUM_FILES + FILES_PER_DIR - 1) / FILES_PER_DIR)
#define	FILE_SIZE	1024

int phase1ticks = 0;
int phase2ticks = 0;
int phase3ticks = 0;
int phase4ticks = 0;

char buf[FILE_SIZE];

extern int alfs_diskreads, alfs_diskwrites;

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
   uint devno, superblkno;
   int val;
   char devname[20];
   FILE *fp;

   if (argc != 2) {
      printf ("Usage: %s <devname>\n", argv[0]);
      exit (0);
   }

   devno = atoi(argv[1]);
   sprintf (devname, "/dev%d", devno);
   fp = fopen (devname, "r");
   assert (fp != NULL);
   fscanf (fp, "ALFS file system definition\n");
   ret = fscanf (fp, "devno: %d\n", &val);
   assert ((ret == 1) && (val == devno));
   ret = fscanf (fp, "superblkno: %d\n", &val);
   assert ((ret == 1) && (val > 0));
   superblkno = val;

   alfs_mountFS (devno, superblkno);

   assert (NUM_FILES == (FILES_PER_DIR * NUM_DIRS));

   for (i=0; i<NUM_DIRS; i++) {
      sprintf (name, "%d", i);
      ret = alfs_mkdir (name, 0777);
      if (ret != OK) {
         printf ("bad return from alfs_mkdir (%s): %d\n", name, ret);
      }
      assert (ret == OK);
   }

   printf ("Small benchmark starting\n");

   /* Phase 1 -- allocate and write */
   alfs_unmountFS();
   alfs_mountFS (devno, superblkno);
   startdiskreads = alfs_diskreads;
   startdiskwrites = alfs_diskwrites;
   starttick = ae_gettick ();
   for (i=0; i<NUM_DIRS; i++) {
      for (j=0; j<FILES_PER_DIR; j++) {
         sprintf (name, "%d/%d", i, j);
         fd = alfs_open (name, (OPT_CREAT|OPT_RDWR), 0777);
         assert (fd > 0);
         ret = alfs_write (fd, buf, FILE_SIZE);
         if (ret != FILE_SIZE) {
            printf ("bad return from alfs_write: %d\n", ret);
         }
         assert (ret == FILE_SIZE);
         ret = alfs_close (fd);
         assert (ret == OK);
      }
   }
   phase1ticks = ae_gettick() - starttick;
   assert (phase1ticks != 0);
   tmpval = (double) phase1ticks * (double) ae_getrate() / (double) 1000000.0;
   printf ("Phase 1 (create/write): disk reads %d, writes %d, secs %.2f, files/sec %.1f\n", (alfs_diskreads - startdiskreads), (alfs_diskwrites - startdiskwrites), tmpval, ((double) NUM_FILES / tmpval));

   /* Phase 2 -- read */
   alfs_unmountFS();
   alfs_mountFS (devno, superblkno);
   startdiskreads = alfs_diskreads;
   startdiskwrites = alfs_diskwrites;
   starttick = ae_gettick ();
   for (i=0; i<NUM_DIRS; i++) {
      for (j=0; j<FILES_PER_DIR; j++) {
         sprintf (name, "%d/%d", i, j);
         fd = alfs_open (name, (OPT_RDONLY), 0777);
         if (fd < 0) {
            printf ("bad return from alfs_open %s: %d\n", name, fd);
         }
         assert (fd > 0);
         ret = alfs_read (fd, buf, FILE_SIZE);
         assert (ret == FILE_SIZE);
         ret = alfs_close (fd);
         assert (ret == OK);
      }
   }
   phase2ticks = ae_gettick() - starttick;
   assert (phase2ticks != 0);
   tmpval = (double) phase2ticks * (double) ae_getrate() / (double) 1000000.0;
   printf ("Phase 2 (read): disk reads %d, writes %d, secs %.2f, files/sec %.1f\n", (alfs_diskreads - startdiskreads), (alfs_diskwrites - startdiskwrites), tmpval, ((double) NUM_FILES / tmpval));

   /* Phase 3 -- overwrite */
   alfs_unmountFS();
   alfs_mountFS (devno, superblkno);
   startdiskreads = alfs_diskreads;
   startdiskwrites = alfs_diskwrites;
   starttick = ae_gettick ();
   for (i=0; i<NUM_DIRS; i++) {
      for (j=0; j<FILES_PER_DIR; j++) {
         sprintf (name, "%d/%d", i, j);
         fd = alfs_open (name, (OPT_RDWR), 0777);
         assert (fd > 0);
         ret = alfs_write (fd, buf, FILE_SIZE);
         assert (ret == FILE_SIZE);
         ret = alfs_close (fd);
         assert (ret == OK);
      }
   }
   phase3ticks = ae_gettick() - starttick;
   assert (phase3ticks != 0);
   tmpval = (double) phase3ticks * (double) ae_getrate() / (double) 1000000.0;
   printf ("Phase 3 (overwrite): disk reads %d, writes %d, secs %.2f, files/sec %.1f\n", (alfs_diskreads - startdiskreads), (alfs_diskwrites - startdiskwrites), tmpval, ((double) NUM_FILES / tmpval));

   /* Phase 4 -- remove */
   alfs_unmountFS();
   alfs_mountFS (devno, superblkno);
   startdiskreads = alfs_diskreads;
   startdiskwrites = alfs_diskwrites;
   starttick = ae_gettick ();
   for (i=0; i<NUM_DIRS; i++) {
      for (j=0; j<FILES_PER_DIR; j++) {
         sprintf (name, "%d/%d", i, j);
         ret = alfs_unlink (name);
         assert (ret == OK);
      }
   }
   phase4ticks = ae_gettick() - starttick;
   assert (phase4ticks != 0);
   tmpval = (double) phase4ticks * (double) ae_getrate() / (double) 1000000.0;
   printf ("Phase 4 (remove): disk reads %d, writes %d, secs %.2f, files/sec %.1f\n", (alfs_diskreads - startdiskreads), (alfs_diskwrites - startdiskwrites), tmpval, ((double) NUM_FILES / tmpval));

   for (i=0; i<NUM_DIRS; i++) {
      sprintf (name, "%d", i);
      ret = alfs_rmdir (name);
      assert (ret == OK);
   }

   alfs_unmountFS();

   {extern int alfs_alloced;
   printf ("Done (alfs_alloced %d)\n", alfs_alloced);
   }

   exit (0);
}

