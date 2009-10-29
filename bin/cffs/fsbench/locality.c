
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

/* Micro-Benchmark #1: Testing locality of reference patterns
   -- We are looking at two types of sequentiality here:
      - spatial (i.e., large files).  This looks at the layout of files
        on disk
      - logical (i.e., files in the same directory).  This looks at how
        related files are laid w.r.t. one another.
      - should add temporal locality benchmark
*/


#include "dtangbm.h"
#include "utility.h"

extern int reps;
extern int blockSize;
extern int bufCacheSize;

void
spatialSeq(struct spa_data *seqData)
/* test spatial locality for sequential access patterns */
{
  char *buf = (char *) myMalloc(blockSize);
  int i, j, fd;
  struct timeval startTime, endTime;
  int fileSize = (bufCacheSize / blockSize) * 5;
  char fname[20];
  int ret;

#ifdef DEBUG
  printf("Spatial Locality: Sequential Access\n");
#endif

  /* initialize variables */
  sprintf(fname, "seq.%d", getpid());      /* name of file */

  /* loop to repeat experiment for statistical significance */
  for (j = 0; j < reps; j++) {
    flushBuffer();

    /* create the file */
#ifdef DEBUG3
    printf("seq: open\n");
#endif
    if ((fd = open(fname, O_CREAT | O_TRUNC | O_RDWR, S_IRWXU)) < 0) {
      perror("spatialSeq, create");
      exit(1);
    }

    /* write the file one f.s. block at a time */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < fileSize; i++) {
#ifdef DEBUG3
      printf("seq: write\n");
#endif
      if (write(fd, buf, blockSize) < blockSize) {
	perror("spatialSeq, write");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    seqData->writeAllocTime[j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("fileSize: %d, write allocate time: %f\n", fileSize,
	   seqData->writeAllocTime[j]);
#endif

    /* flush buffer cache */
    flushBuffer();

    /* read the file one f.s. block at a time */
#ifdef DEBUG3
    printf("seq: lseek\n");
#endif
    ret = lseek(fd, 0, SEEK_SET);
    if (ret < 0) {
       printf ("lseek failed: ret %d\n", ret);
    }
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < fileSize; i++) {
#ifdef DEBUG3
    printf("seq: read\n");
#endif
      if ((ret = read(fd, buf, blockSize)) < blockSize) {
        printf ("read failed: ret %d (expected %d)\n", ret, blockSize);
	perror("spatialSeq, read");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    seqData->readTime[j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("fileSize: %d, read time: %f\n", fileSize, seqData->readTime[j]);
#endif

    /* flush the cache */
    flushBuffer();

    /* write the file one f.s. block at a time (no block allocation time) */
#ifdef DEBUG3
    printf("seq: lseek2\n");
#endif
    lseek(fd, 0, SEEK_SET);
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < fileSize; i++) {
#ifdef DEBUG3
    printf("seq: write2\n");
#endif
      if (write(fd, buf, blockSize) < blockSize) {
	perror("spatialSeq, overwrite");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    seqData->writeTime[j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("fileSize: %d, overwrite time: %f\n", fileSize,seqData->writeTime[j]);
#endif

    /* clean up */
    close(fd);
#ifdef DEBUG3
    printf("seq: unlink\n");
#endif
    if (unlink(fname) < 0) {
      perror("spatialSeq, unlink");
      exit(1);
    }
  }
  free(buf);
}

void
spatialRand(struct spa_data *seqData)
/* test spatial locality for random access patterns */
{
  char *buf = (char *) myMalloc(blockSize);
  int i, j, fd;
  struct timeval startTime, endTime;
  int fileSize = (bufCacheSize / blockSize) * 5;
  char fname[20];
  int *addrArray = (int *) myMalloc(fileSize * sizeof(int));

#ifdef DEBUG
  printf("Spatial Locality: Random Access\n");
#endif

  /* initialize variables */
  sprintf(fname, "seq.%d", getpid());           /* file name */

  /* loop to repeat experiment for statistical significance */
  for (j = 0; j < reps; j++) {
    flushBuffer();
    createPerm(&addrArray, fileSize, blockSize, fileSize);

    /* create the file */
#ifdef DEBUG3
    printf("rand: open\n");
#endif
    if ((fd = open(fname, O_CREAT | O_TRUNC | O_RDWR, S_IRWXU)) < 0) {
      perror("spatialRand, create");
      exit(1);
    }

    /* write the file one f.s. block at a time */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < fileSize; i++) {
#ifdef DEBUG3
    printf("rand: lseek\n");
#endif
      lseek(fd, addrArray[i], SEEK_SET);
#ifdef DEBUG3
    printf("rand: write\n");
#endif
      if (write(fd, buf, blockSize) < blockSize) {
	perror("spatialRand, write");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    seqData->writeAllocTime[j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("fileSize: %d, write allocate time: %f\n", fileSize,
	   seqData->writeAllocTime[j]);
#endif

    /* flush buffer cache */
    flushBuffer();

    /* read the file one f.s. block at a time, in random order */
    createPerm(&addrArray, fileSize, blockSize, fileSize);
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < fileSize; i++) {
#ifdef DEBUG3
    printf("rand: lseek2\n");
#endif
      lseek(fd, addrArray[i], SEEK_SET);
#ifdef DEBUG3
    printf("rand: read\n");
#endif
      if (read(fd, buf, blockSize) < blockSize) {
	perror("spatialRand, read");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    seqData->readTime[j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("fileSize: %d, read time: %f\n", fileSize, seqData->readTime[j]);
#endif

    /* flush the cache */
    flushBuffer();

    /* write the file one f.s. block at a time (no block allocation time),
       in random order */
    createPerm(&addrArray, fileSize, blockSize, fileSize);
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < fileSize; i++) {
#ifdef DEBUG3
    printf("rand: lseek3\n");
#endif
      lseek(fd, addrArray[i], SEEK_SET);
#ifdef DEBUG3
    printf("rand: write2\n");
#endif
      if (write(fd, buf, blockSize) < blockSize) {
	perror("spatialRand, overwrite");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    seqData->writeTime[j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("fileSize: %d, overwrite time: %f\n", fileSize,seqData->writeTime[j]);
#endif

    /* clean up */
    close(fd);
#ifdef DEBUG3
    printf("rand: unlink\n");
#endif
    if (unlink(fname) < 0) {
      perror("spatialRand, unlink");
      exit(1);
    }
  }
  free(addrArray);
}

void
logical(struct log_data *logData)
/* the point of this benchmark is to measure data access time of
   files in different directories -- along the lines of a grep */
{
  char *buf = (char *) myMalloc(blockSize);
  int *fileSize;
  int i, j, k, count, fd, expo, size, index;
  struct timeval startTime, endTime;
  int totalNum;
  char **fname, **dname;
  int *addrArray;
  int numDirs, numRealDirs, dirNum;
  int *eachDir;

#ifdef DEBUG
  printf("Logical\n");
#endif

  /* figure out how many files to create */
  for (expo = 1, size = 2; size < bufCacheSize; size *= 2, expo++) ;
  for (totalNum = 0, i = expo, j = size; j > 64; j /= 2, i--)
    totalNum += (expo - i + 1);

  /* create the name array */
  fileSize = (int *) myMalloc (totalNum * sizeof(int));
  addrArray = (int *) myMalloc(sizeof(int) * totalNum);
  createPerm(&addrArray, totalNum, 1, totalNum);

  /* figure out the size of each file */
  logData->totalSize = 0;
  for (count = 0, i = expo, j = size; j > 64; j /= 2, i--) 
    for (k = 0; k < (expo - i + 1); k++) {
      fileSize[addrArray[count++]] = j;
      logData->totalSize += j;
    }
  logData->numFiles = totalNum;

  /* name the files */
  createFiles(totalNum, &dname, &fname, &numDirs);
#ifdef DEBUG
  printf("logical, %s\n", dname[0]);
#endif

  /* we want to create files in such a way so that we know the
     creation order within a directory, but not the overall 
     creation order -- in other words, we want to randomly choose
     a directory in which to create a file until all the files
     are created */

  /* random numbers for choosing the dir */
  createPerm(&addrArray, totalNum, 1, totalNum);

  /* initialize variables which keep track of which files in each 
     directory have been created already so we can create the files 
     in each dir in order */
  numRealDirs = (totalNum / MAXNUMFILESINDIR) + 1;
  eachDir = (int *) myMalloc(sizeof(int) * numRealDirs);
  for (i = 0; i < numRealDirs; i++)
    eachDir[i] = 0;

  /* loop to create the files */
  for (i = 0; i < totalNum; i++) {
    /* figure out which directory -- if the first choice directory is full,
       just increment until we find one that isn't */
    dirNum = addrArray[i] % numRealDirs;
    while ((eachDir[dirNum] == MAXNUMFILESINDIR) ||
	   (((dirNum * MAXNUMFILESINDIR) + eachDir[dirNum]) >= totalNum)) {
      dirNum++;
      if (dirNum == numRealDirs)
	dirNum = 0;
    }
    index = dirNum * MAXNUMFILESINDIR + eachDir[dirNum];
    eachDir[dirNum]++;

    /* create the file */
#ifdef DEBUG3
    printf("logical: open\n");
#endif
    if ((fd = open(fname[index], O_CREAT | O_TRUNC | O_RDWR, S_IRWXU)) < 0) {
      perror("logical, create");
      exit(1);
    }

    /* write to the file one block at a time so that it's the appropriate
       size */
    for (j = 0; j < fileSize[index]; ) {
      if (fileSize[index] - j > blockSize) {
#ifdef DEBUG3
    printf("logical: write\n");
#endif
	if (write(fd, buf, blockSize) < blockSize) {
	  perror("logical, write");
	  exit(1);
	}
	j += blockSize;
      } else {
#ifdef DEBUG3
    printf("logical: write2\n");
#endif
	if (write(fd, buf, (fileSize[index] - j)) < (fileSize[index] - j)) {
	  perror("logical, write2");
	  exit(1);
	}
	j = fileSize[index];
      }
    }
    close(fd);
  }
  free(eachDir);
  
  /* main loop -- repeat _reps_ time for accuracy */
  for (k = 0; k < reps; k++) {
    flushAll();

    /* measure open / close time */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < totalNum; i++) {
#ifdef DEBUG3
    printf("logical: open2\n");
#endif
      if ((fd = open(fname[i], O_RDWR, S_IRWXU)) < 0) {
	perror ("logical, open");
	printf ("failed open: name %s, i %d, totalNum %d, ret %d\n", fname[i], i, totalNum, fd);
	exit(1);
      }
      close(fd);
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    logData->openCloseTime[k] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("openCloseTime: %f\n", logData->openCloseTime[k]);
#endif

    /* flush caches */
    flushAll();

    /* measure open / close, random within dir time */
    createPerm(&addrArray, totalNum, 1, MAXNUMFILESINDIR);
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < totalNum; i++) {
#ifdef DEBUG3
    printf("logical: open3\n");
#endif
      if ((fd = open(fname[addrArray[i]], O_RDWR, S_IRWXU)) < 0) {
	perror("logical, open randDir");
	exit(1);
      }
      close(fd);
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    logData->openCloseRandDirTime[k] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("openCloseRandDirTime: %f\n", logData->openCloseRandDirTime[k]);
#endif

    /* flush caches */
    flushAll();

    /* measure open / close, random time */
    createPerm(&addrArray, totalNum, 1, totalNum);
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < totalNum; i++) {
#ifdef DEBUG3
    printf("logical: open4\n");
#endif
      if ((fd = open(fname[addrArray[i]], O_RDWR, S_IRWXU)) < 0) {
	perror("logical, open rand");
	exit(1);
      }
      close(fd);
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    logData->openCloseRandTime[k] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("openCloseRandTime: %f\n", logData->openCloseRandTime[k]);
#endif

    /* flush caches */
    flushAll();

    /* measure read time */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < totalNum; i++) {
#ifdef DEBUG3
    printf("logical: open5\n");
#endif
/*
      printf ("opening %s for read\n", fname[i]);
*/
      if ((fd = open(fname[i], O_RDWR, S_IRWXU)) < 0) {
	perror("logical, open - read");
        printf ("fd %d, i %d, fname[i] %s\n", fd, i, fname[i]);
	exit(1);
      }
      /* read file one block at a time */
      for (j = 0; j < fileSize[i]; ) {
	if (fileSize[i] - j > blockSize) {
#ifdef DEBUG3
    printf("logical: read\n");
#endif
	  if (read(fd, buf, blockSize) < blockSize) {
	    perror("logical, read");
	    exit(1);
	  }
	  j += blockSize;
	} else {
#ifdef DEBUG3
    printf("logical: read2\n");
#endif
	  if (read(fd, buf, (fileSize[i] - j)) < (fileSize[i] - j)) {
	    perror("logical, read2");
	    exit(1);
	  }
	  j = fileSize[i];
	}
      }
      close(fd);
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    logData->readTime[k] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("readTime: %f\n", logData->readTime[k]);
#endif

    /* flush caches */
    flushAll();

    /* measure read time, random within a directory */
    createPerm(&addrArray, totalNum, 1, MAXNUMFILESINDIR);
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < totalNum; i++) {
#ifdef DEBUG3
    printf("logical: open5\n");
#endif
      if ((fd = open(fname[addrArray[i]], O_RDWR, S_IRWXU)) < 0) {
	perror("logical, open - read randDir");
	exit(1);
      }
      for (j = 0; j < fileSize[addrArray[i]]; ) {
	if (fileSize[addrArray[i]] - j > blockSize) {
#ifdef DEBUG3
    printf("logical: read3\n");
#endif
	  if (read(fd, buf, blockSize) < blockSize) {
	    perror("logical, read randDir");
	    exit(1);
	  }
	  j += blockSize;
	} else {
#ifdef DEBUG3
    printf("logical: read4\n");
#endif
	  if (read(fd, buf, (fileSize[addrArray[i]] - j)) < 
	      (fileSize[addrArray[i]] - j)) {
	    perror("logical, read randDir 2");
	    exit(1);
	  }
	  j = fileSize[addrArray[i]];
	}
      }
      close(fd);
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    logData->readRandDirTime[k] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("readRandDirTime: %f\n", logData->readRandDirTime[k]);
#endif

    /* flush caches */
    flushAll();

    /* measure read time, random */
    createPerm(&addrArray, totalNum, 1, totalNum);
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < totalNum; i++) {
#ifdef DEBUG3
    printf("logical: open6\n");
#endif
      if ((fd = open(fname[addrArray[i]], O_RDWR, S_IRWXU)) < 0) {
	perror("logical, open -- read rand");
	exit(1);
      }
      for (j = 0; j < fileSize[addrArray[i]]; ) {
	if (fileSize[addrArray[i]] - j > blockSize) {
#ifdef DEBUG3
    printf("logical: read5\n");
#endif
	  if (read(fd, buf, blockSize) < blockSize) {
	    perror("logical, read rand");
	    exit(1);
	  }
	  j += blockSize;
	} else {
#ifdef DEBUG3
    printf("logical: read6\n");
#endif
	  if (read(fd, buf, (fileSize[addrArray[i]] - j)) < 
	      (fileSize[addrArray[i]] - j)) {
	    perror("logical, read rand 2");
	    exit(1);
	  }
	  j = fileSize[addrArray[i]];
	}
      }
      close(fd);
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    logData->readRandTime[k] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("readRandTime: %f\n", logData->readRandTime[k]);
#endif

    /* flush caches */
    flushAll();

    /* measure write time */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < totalNum; i++) {
#ifdef DEBUG3
    printf("logical: open7\n");
#endif
      if ((fd = open(fname[i], O_RDWR, S_IRWXU)) < 0) {
	perror("logical, open -- write");
	exit(1);
      }
      for (j = 0; j < fileSize[i]; ) {
	if (fileSize[i] - j > blockSize) {
#ifdef DEBUG3
    printf("logical: wrte3\n");
#endif
	  if (write(fd, buf, blockSize) < blockSize) {
	    perror("logical, write");
	    exit(1);
	  }
	  j += blockSize;
	} else {
#ifdef DEBUG3
    printf("logical: write4\n");
#endif
	  if (write(fd, buf, (fileSize[i] - j)) < (fileSize[i] - j)) {
	    perror("logical, write2");
	    exit(1);
	  }
	  j = fileSize[i];
	}
      }
      close(fd);
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    logData->writeTime[k] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("writeTime: %f\n", logData->writeTime[k]);
#endif

    /* flush caches */
    flushAll();

    /* measure write time, random within a directory */
    createPerm(&addrArray, totalNum, 1, MAXNUMFILESINDIR);
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < totalNum; i++) {
#ifdef DEBUG3
    printf("logical: open8\n");
#endif
      if ((fd = open(fname[addrArray[i]], O_RDWR, S_IRWXU)) < 0) {
	perror("logical, open -- write randDir");
        printf ("failed open: name %s fd %d\n", fname[addrArray[i]], fd);
	exit(1);
      }
      for (j = 0; j < fileSize[addrArray[i]]; ) {
	if (fileSize[addrArray[i]] - j > blockSize) {
#ifdef DEBUG3
    printf("logical: write5\n");
#endif
	  if (write(fd, buf, blockSize) < blockSize) {
	    perror("logical, write randDir");
	    exit(1);
	  }
	  j += blockSize;
	} else {
#ifdef DEBUG3
    printf("logical: write6\n");
#endif
	  if (write(fd, buf, (fileSize[addrArray[i]] - j)) < 
	      (fileSize[addrArray[i]] - j)) {
	    perror("logical, write randDir 2");
	    exit(1);
	  }
	  j = fileSize[addrArray[i]];
	}
      }
      close(fd);
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    logData->writeRandDirTime[k] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("writeRandDirTime: %f\n", logData->writeRandDirTime[k]);
#endif

    /* flush caches */
    flushAll();

    /* measure write time, random */
    createPerm(&addrArray, totalNum, 1, totalNum);
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < totalNum; i++) {
#ifdef DEBUG3
    printf("logical: open9\n");
#endif
      if ((fd = open(fname[addrArray[i]], O_RDWR, S_IRWXU)) < 0) {
	perror("logical, open -- write rand");
	exit(1);
      }
      for (j = 0; j < fileSize[addrArray[i]]; ) {
	if (fileSize[addrArray[i]] - j > blockSize) {
#ifdef DEBUG3
    printf("logical: write10\n");
#endif
	  if (write(fd, buf, blockSize) < blockSize) {
	    perror("logical, write rand");
	    exit(1);
	  }
	  j += blockSize;
	} else {
#ifdef DEBUG3
    printf("logical: write11\n");
#endif
	  if (write(fd, buf, (fileSize[addrArray[i]] - j)) < 
	      (fileSize[addrArray[i]] - j)) {
	    perror("logical, write rand 2");
	    exit(1);
	  }
	  j = fileSize[addrArray[i]];
	}
      }
      close(fd);
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    logData->writeRandTime[k] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("writeRandTime: %f\n", logData->writeRandTime[k]);
#endif
  }

  /* clean up, delete all files, free memory */
  for (i = 0; i < totalNum; i++) {
#ifdef DEBUG3
    printf("logical: unlink\n");
#endif
    if (unlink(fname[i]) < 0) {
      perror("logical, unlink");
      exit(1);
    }
    free(fname[i]);
  }
  free(fname);
  free(fileSize);
  free(addrArray);

  for (i = (numDirs - 1); i >= 0; i--) {
#ifdef DEBUG3
    printf("logical: rmdir\n");
#endif
    if (rmdir(dname[i]) < 0) {
      perror("logical, rmdir");
      exit(1);
    }
    free(dname[i]);
  }
  free(dname);
  free(buf);
}
