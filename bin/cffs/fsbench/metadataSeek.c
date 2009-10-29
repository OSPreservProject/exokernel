
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

/* Micro-Benchmark: Meta-Data Seek Time
   The other meta-data benchmarks are all very Unix / FFS oriented.
   For a more general meta-data benchmark, we just go through various
   meta-data operations, and check the number of seeks required to complete
   each operation, averaged over several repetitions
*/

/*
#define DIRECTORIES_TOO
*/

#include "dtangbm.h"
#include "utility.h"

extern int bufCacheSize, inodeCacheSize, dnlcSize;
extern int reps;

void
metadataTime(struct time_data *timeData, char **fname, int numFiles)
{
  int i, j;
  int fd = -1;
  int *addr;
  struct timeval startTime, endTime;
  struct stat buf;

#ifdef DEBUG
  printf("Metadata Time\n");
#endif

  /* initialization */
  addr = (int *) myMalloc(numFiles * sizeof(int));

  for (j = 0; j < reps; j++) {
    /* flush buffers */
    flushAll();

    /* create the files -- sequentially */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if ((fd = open(fname[i], O_CREAT | O_TRUNC | O_RDWR, S_IRWXU)) == -1) {
	perror("metadataTime, create");
	exit(1);
      }
      close(fd);
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[CREATESEQ][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, create: %f\n",numFiles,timeData->times[CREATESEQ][j]);
#endif

    /* flush buffers */
    flushAll();
    
    /* stat the files -- sequentially */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (stat(fname[i], &buf) == -1) {
	perror("metadataTime, stat seq");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[STATFSEQ][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, statseq: %f\n",numFiles,timeData->times[STATFSEQ][j]);
#endif

    /* flush buffers */
    flushAll();
    
    /* stat the files -- rand */
    createPerm(&addr, numFiles, 1, numFiles);
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (stat(fname[addr[i]], &buf) == -1) {
	perror("metadataTime, stat rand");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[STATFRAND][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, stat rand: %f\n",numFiles,timeData->times[STATFRAND][j]);
#endif

    /* flush buffers */
    flushAll();
    
    /* delete the files -- sequentially */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (unlink(fname[i]) == -1) {
	perror("metadataTime, delete seq");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[DELETESEQ][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, seq. delete: %f\n", numFiles, 
	   timeData->times[DELETESEQ][j]);
#endif

    /* flush buffers */
    flushAll();
    
    /* create the files, followed by a sync */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if ((fd = open(fname[i], O_CREAT | O_TRUNC | O_RDWR, S_IRWXU)) == -1) {
	perror("metadataTime, create sync");
	exit(1);
      }
      close(fd);
      sync();
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[CREATESYNC][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, sync create: %f\n", numFiles, 
	   timeData->times[CREATESYNC][j]);
#endif

    /* flush buffers */
    flushAll();
    
    /* delete the files -- randomly */
    createPerm(&addr, numFiles, 1, numFiles);
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (unlink(fname[addr[i]]) == -1) {
	perror("metadataTime, delete rand");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[DELETERAND][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, rand delete: %f\n", numFiles, 
	   timeData->times[DELETERAND][j]);
#endif

    /* flush buffers */
    flushAll();
    
    /* create the files */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if ((fd = open(fname[i], O_CREAT | O_TRUNC | O_RDWR, S_IRWXU)) == -1) {
	perror("metadataTime, create2");
	exit(1);
      }
      close(fd);
      sync();
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[CREATE2][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, create2: %f\n", numFiles, 
	   timeData->times[CREATE2][j]);
#endif

    /* flush buffers */
    flushAll();
    
    /* delete the files -- sequentially */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (unlink(fname[i]) == -1) {
	perror("metadataTime, delete seq sync");
	exit(1);
      }
      sync();
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[DELETESEQSYNC][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, seq. delete: %f\n", numFiles, 
	   timeData->times[DELETESEQSYNC][j]);
#endif

    
    /* flush buffers */
    flushAll();
    
    /* create the files */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if ((fd = open(fname[i], O_CREAT | O_TRUNC | O_RDWR, S_IRWXU)) == -1) {
	perror("metadataTime, create3");
	exit(1);
      }
      close(fd);
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[CREATE3][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, create3: %f\n", numFiles, timeData->times[CREATE3][j]);
#endif

    /* flush buffers */
    flushAll();
    
    /* delete the files -- randomly, followed by a sync */
    createPerm(&addr, numFiles, 1, numFiles);
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (unlink(fname[addr[i]]) == -1) {
	perror("metadataTime, delete rand sync");
	exit(1);
      }
      sync();
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[DELETERANDSYNC][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, sync rand delete: %f\n", numFiles, 
	   timeData->times[DELETERANDSYNC][j]);
#endif

    /* flush buffers */
    flushAll();

    /* make the directories -- sequentially */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (mkdir(fname[i], S_IRWXU) == -1) {
	perror("metadataTime, mkdir");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[MKDIRSEQ][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, mkdir: %f\n", numFiles, timeData->times[MKDIRSEQ][j]);
#endif

    /* flush buffers */
    flushAll();
    
    /* stat the dirs -- sequentially */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (stat(fname[i], &buf) == -1) {
	perror("metadataTime, stat dir seq");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[STATDSEQ][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, statdseq: %f\n",numFiles,timeData->times[STATDSEQ][j]);
#endif

    /* flush buffers */
    flushAll();
    
    /* stat the dirs -- rand */
    createPerm(&addr, numFiles, 1, numFiles);
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (stat(fname[addr[i]], &buf) == -1) {
	perror("metadataTime, stat dir rand");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[STATDRAND][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, statdrand: %f\n",numFiles,timeData->times[STATDRAND][j]);
#endif

    /* flush buffers */
    flushAll();
    
    /* remove the directories -- sequentially */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (rmdir(fname[i]) == -1) {
	perror("metadataTime, rmdir seq");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[RMDIRSEQ][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, seq. rmdir: %f\n", numFiles, 
	   timeData->times[RMDIRSEQ][j]);
#endif

    /* flush buffers */
    flushAll();
    
    /* make the directories, followed by a sync */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (mkdir(fname[i], S_IRWXU) == -1) {
	perror("metadataTime, mkdir sync");
	exit(1);
      }
      sync();
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[MKDIRSYNC][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, sync mkdir: %f\n", numFiles, 
	   timeData->times[MKDIRSYNC][j]);
#endif

    /* flush buffers */
    flushAll();
    
    /* remove the directories -- randomly */
    createPerm(&addr, numFiles, 1, numFiles);
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (rmdir(fname[addr[i]]) == -1) {
	perror("metadataTime, rmdir rand");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[RMDIRRAND][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, rand rmdir: %f\n", numFiles, 
	   timeData->times[RMDIRRAND][j]);
#endif

    /* flush buffers */
    flushAll();
    
    /* make the directories */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (mkdir(fname[i], S_IRWXU) == -1) {
	perror("metadataTime, mkdir2");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[MKDIR2][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, mkdir2: %f\n", numFiles, 
	   timeData->times[MKDIR2][j]);
#endif

    /* flush buffers */
    flushAll();
    
    /* remove the directories -- sequentially with sync */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (rmdir(fname[i]) == -1) {
	perror("metadataTime, rmdir seq sync");
	exit(1);
      }
      sync();
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[RMDIRSEQSYNC][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, sync seq. rmdir: %f\n", numFiles, 
	   timeData->times[RMDIRSEQSYNC][j]);
#endif

    /* flush buffers */
    flushAll();
    
    /* make the directories */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (mkdir(fname[i], S_IRWXU) == -1) {
	perror("metadataTime, mkdir3");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[MKDIR3][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, mkdir3: %f\n", numFiles, 
	   timeData->times[MKDIR3][j]);
#endif

    
    /* flush buffers */
    flushAll();
    
    /* remove the directories -- randomly, with sync */
    createPerm(&addr, numFiles, 1, numFiles);
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (rmdir(fname[addr[i]]) == -1) {
	perror("metadataTime, rmdir rand sync");
	exit(1);
      }
      sync();
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[RMDIRRANDSYNC][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, rand rmdir sync: %f\n", numFiles, 
	   timeData->times[RMDIRRANDSYNC][j]);
#endif

    /* flush buffers */
    flushAll();

    /* sync */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) 
      sync();
    gettimeofday(&endTime, (struct timezone *) NULL);
    timeData->times[SYNC][j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, sync: %f\n", numFiles, timeData->times[SYNC][j]);
#endif
  }
}
