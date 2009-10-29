
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

#include <assert.h>

#include "dtangbm.h"
#include "functions.h"
#include "utility.h"

char * FSname = NULL;

int bufCacheSize = ALFS_BUFCACHE_SIZE;
#if 0
int inodeCacheSize = ALFS_BUFCACHE_SIZE / 128; /* ALFS_DINODE_SIZE */
#else
int inodeCacheSize = ALFS_BUFCACHE_SIZE / 512; /* ALFS_DINODE_SIZE */
#endif
int blockSize = BLOCK_SIZE;
int reps = 0;

uint devno, superblkno;

char *ops[21] = {
  "create",
  "delete seq",
  "create, sync",
  "delete rand",
  "create",
  "delete seq, sync",
  "create",
  "delete rand, sync",
  "mkdir",
  "rmdir seq",
  "mkdir, sync",
  "rmdir rand",
  "mkdir",
  "rmdir seq, sync",
  "mkdir",
  "rmdir rand, sync",
  "sync",
  "stat files, seq",
  "stat files, rand",
  "stat dirs, seq",
  "stat dirs, rand" };

int main (int argc, char **argv) {

   int fileSize;
   int i, j;

   int ret, val;
   char devname[20];
   FILE *fp;

  /* benchmark setup variables */
  int numFiles, numdirs;
  char **dname, **fname;

     /* result variables */
  struct spa_data spaSeqData, spaRandData;
  struct log_data logData;
  struct time_data timeData;
/*
  struct sub_data inodeAccess, dirCreate;
  struct sub3_data inodeCreate;
  struct dnlc_data dnlcData;
  struct readahead_data readaheadData;
*/

   if (argc != 3) {
      printf ("Usage: %s <devname> <reps>\n", argv[0]);
      exit (0);
   }

   printf ("Starting: %s %s %s\n", argv[0], argv[1], argv[2]);

   FSname = argv[1];

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

   reps = atoi(argv[2]);

   /************************* dtangbm main.c stuff ***********************/

  /* set up files for metadata benchmarks */
  numFiles = inodeCacheSize;
  createFiles(numFiles, &dname, &fname, &numdirs);

  /* malloc memory */
  spaSeqData.writeAllocTime = (double *) myMalloc(sizeof(double) * reps);
  spaSeqData.readTime = (double *) myMalloc(sizeof(double) * reps);
  spaSeqData.writeTime = (double *) myMalloc(sizeof(double) * reps);

  spaRandData.writeAllocTime = (double *) myMalloc(sizeof(double) * reps);
  spaRandData.readTime = (double *) myMalloc(sizeof(double) * reps);
  spaRandData.writeTime = (double *) myMalloc(sizeof(double) * reps);

  logData.openCloseTime = (double *) myMalloc (sizeof(double) * reps);
  logData.openCloseRandDirTime = (double *) myMalloc (sizeof(double) * reps);
  logData.openCloseRandTime = (double *) myMalloc (sizeof(double) * reps);
  logData.readTime = (double *) myMalloc (sizeof(double) * reps);
  logData.readRandDirTime = (double *) myMalloc (sizeof(double) * reps);
  logData.readRandTime = (double *) myMalloc (sizeof(double) * reps);
  logData.writeTime = (double *) myMalloc (sizeof(double) * reps);
  logData.writeRandDirTime = (double *) myMalloc (sizeof(double) * reps);
  logData.writeRandTime = (double *) myMalloc (sizeof(double) * reps);

  for (i = 0; i < NUMOPS; i++)
    timeData.times[i] = (double *) myMalloc (sizeof(double) * reps);

  /* benchmarks, time each benchmark */

printf ("starting benchmarks\n");

  spatialSeq(&spaSeqData);            /* spatial locality, seq. access */
  fileSize = bufCacheSize * 5;
  printf("Spatial Locality, Sequential Access: KBytes / sec (std. dev)\n");
  for (i=0; i<reps; i++) {
     spaSeqData.writeAllocTime[i] = (double) (fileSize * 1000.0) / (spaSeqData.writeAllocTime[i] * 1024.0);
     spaSeqData.readTime[i] = (double) (fileSize * 1000.0) / (spaSeqData.readTime[i] * 1024.0);
     spaSeqData.writeTime[i] = (double) (fileSize * 1000.0) / (spaSeqData.writeTime[i] * 1024.0);
  }
  printf("Write: %f (%f)\n",
          (getavg(spaSeqData.writeAllocTime)),
          (getStdDev(spaSeqData.writeAllocTime)));
  printf("Read: %f (%f)\n",
          (getavg(spaSeqData.readTime)),
          (getStdDev(spaSeqData.readTime)));
  printf("OverWrite: %f (%f)\n",
          (getavg(spaSeqData.writeTime)),
          (getStdDev(spaSeqData.writeTime)));

  spatialRand(&spaRandData);          /* spatial locality, rand. access */
  printf("Spatial Locality, Random Access: KBytes / sec (std.dev)\n");
  for (i=0; i<reps; i++) {
     spaRandData.writeAllocTime[i] = (double) (fileSize * 1000.0) / (spaRandData.writeAllocTime[i] * 1024.0);
     spaRandData.readTime[i] = (double) (fileSize * 1000.0) / (spaRandData.readTime[i] * 1024.0);
     spaRandData.writeTime[i] = (double) (fileSize * 1000.0) / (spaRandData.writeTime[i] * 1024.0);
  }
  printf("Write: %f (%f)\n",
          (getavg(spaRandData.writeAllocTime)),
          (getStdDev(spaRandData.writeAllocTime)));
  printf("Read: %f (%f)\n",
          (getavg(spaRandData.readTime)),
          (getStdDev(spaRandData.readTime)));
  printf("OverWrite: %f (%f)\n",
          (getavg(spaRandData.writeTime)),
          (getStdDev(spaRandData.writeTime)));

  logical(&logData);                  /* logical locality */
  for (i = 0; i < reps; i++) {
    logData.readTime[i] -= logData.openCloseTime[i];
    logData.readRandDirTime[i] -= logData.openCloseRandDirTime[i];
    logData.readRandTime[i] -= logData.openCloseRandTime[i];
    logData.writeTime[i] -= logData.openCloseTime[i];
    logData.writeRandDirTime[i] -= logData.openCloseRandDirTime[i];
    logData.writeRandTime[i] -= logData.openCloseRandTime[i];
  }
  printf("Logical Locality: KBytes / sec (std. dev)\n");
  for (i=0; i<reps; i++) {
     logData.readTime[i] = (double) (logData.totalSize * 1000.0) / (logData.readTime[i] * 1024.0);
     logData.readRandDirTime[i] = (double) (logData.totalSize * 1000.0) / (logData.readRandDirTime[i] * 1024.0);
     logData.readRandTime[i] = (double) (logData.totalSize * 1000.0) / (logData.readRandTime[i] * 1024.0);
     logData.writeTime[i] = (double) (logData.totalSize * 1000.0) / (logData.writeTime[i] * 1024.0);
     logData.writeRandDirTime[i] = (double) (logData.totalSize * 1000.0) / (logData.writeRandDirTime[i] * 1024.0);
     logData.writeRandTime[i] = (double) (logData.totalSize * 1000.0) / (logData.writeRandTime[i] * 1024.0);
  }
  printf("Reading files in creation order: %f (%f)\n",
          (getavg(logData.readTime)),
          (getStdDev(logData.readTime)));
  printf("Reading files in random order within a dir: %f (%f)\n",
          (getavg(logData.readRandDirTime)),
          (getStdDev(logData.readRandDirTime)));
  printf("Reading files in random order: %f (%f)\n",
          (getavg(logData.readRandTime)),
          (getStdDev(logData.readRandTime)));
  printf("Writing files in creation order: %f (%f)\n",
          (getavg(logData.writeTime)),
          (getStdDev(logData.writeTime)));
  printf("Writing files in random order within a dir: %f (%f)\n",
          (getavg(logData.writeRandDirTime)),
          (getStdDev(logData.writeRandDirTime)));
  printf("Writing files in random order: %f (%f)\n",
          (getavg(logData.writeRandTime)),
          (getStdDev(logData.writeRandTime)));

  metadataTime(&timeData, fname, numFiles);            /* metadata */
  printf("Meta-data Operations: op / sec (std.dev)\n");
  for (i = 0; i < NUMOPS; i++) {
    for (j=0; j<reps; j++) {
       timeData.times[i][j] = (double) numFiles * (double) 1000.0 / timeData.times[i][j];
    }
    printf("%s: %f (%f)\n", ops[i], getavg(timeData.times[i]), getStdDev(timeData.times[i]));
  }
  printf("\n");

  /* clean up */
  for (i = 0; i < numFiles; i++)
    free(fname[i]);
  free(fname);
  for (i = numdirs - 1; i >= 0; i--) {
    if (rmdir(dname[i]) < 0) {
      perror("dtangbm, rmdir");
      exit(1);
    }
    free(dname[i]);
  }
  free(dname);

   /*********************** end dtangbm main.c stuff *********************/

   alfs_unmountFS();

   {extern int alfs_alloced;
   printf ("Done (alfs_alloced %d)\n", alfs_alloced);
   }

   exit (0);
}

