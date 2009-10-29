
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

/* utility.c:
   All of the functions for timing, creating hierarchies, a safe 
   malloc, etc. are here.
*/

#include "dtangbm.h"

#include "math.h"

/* external global variables */
extern int bufCacheSize, inodeCacheSize, dnlcSize;
extern int reps;
extern int blockSize;
extern int numFilesFlush;
extern char **dnameFlush, **fnameFlush;
extern int numFilesDNLC;
extern char **fnameDNLC;
extern int *addrArrayFlush;
extern int *addrArrayDNLC;

#if 0
double
diffTime(struct timeval startTime, struct timeval endTime)
/* returns the difference between the two times in ms */
{
  double retTime;

  retTime = ((endTime.tv_sec - startTime.tv_sec) * 1000.0 +
	     (endTime.tv_usec - startTime.tv_usec) / 1000.0);
  return retTime;
}
#endif

/* myRealloc -- checks for failed allocation */
void *
myRealloc(void *ptr, int size)
{
  void *retPtr = realloc(ptr, size);

  if (retPtr == (void *) 0) {
    perror("myRealloc");
    exit(1);
  }
  return retPtr;
}

/* myMalloc -- checks for failed allocation */
void *
myMalloc(size_t size)
{
  void *retPtr = malloc(size);

  if (retPtr == (void *) 0) {
    perror("myMalloc");
    exit(1);
  }
  return retPtr;
}

void
createPerm(int **addrArray, int sizePerm, int bSize, int size)
/* creates a permutation between 0 and size, for sizePerm / size
   times, all in addrArray (which is of size sizePerm).  bSize is
   a multiplicative factor -- a permutationof 0, 1, 2 will turn
   into a permutation of 0, 1 * bSize, 2 * bSize. */
{
  int totSize = (size / 8) + 1;
  unsigned char *bucket = (unsigned char *)myMalloc(totSize);
  int i, j;
  int num;
  int loop = (sizePerm % size == 0) ? sizePerm / size : sizePerm / size + 1;
  int lsize = size;

  /* loop depending on how many permutations fit into the total
     size of what is needed -- sizePerm / size */
  for (i = 0; i < loop; i++) {
    /* initialize bucket */
    for (j = 0; j < totSize; j++)
      bucket[j] = 0;
    /* decide on how big the permutation is (max number) */
    if ((i * size + size) > sizePerm)
      lsize = sizePerm - i * size;
    /* create permutation */
    for (j = 0; j < lsize; j++) {
      num = rand() % lsize;
      do {
	num++;
	if (num >= lsize)
	  num = 0;
      } while (bucket[num / 8] & (1 << (num % 8)));
      bucket[num / 8] |= (1 << (num % 8));
      (*addrArray)[i * size + j] = (num + i * size) * bSize;
    }
  }
  free(bucket);
}

void
createFiles(int numFiles, char ***dirs, char ***files, int *numdirs)
/* creates a hierarchy with numFiles in directories.
   Puts names of the dirs in ***dirs, and names of the files in ***files,
   and the total number of directories in *numdirs */
{
  static int count = 0;
  int i, j, k;
  int numlevels;
  int startindex, endindex, temp, index;

#ifdef DEBUG2
  printf("createFiles %d\n", numFiles);
#endif
  /* figure out how many levels of directories are needed */
  for (numlevels = 1, j = MAXNUMFILESINDIR; j < numFiles; numlevels++,
       j *= MAXNUMFILESINDIR);

  /* figure out how many directories are needed */
  for (*numdirs = 0, i = 0, j = 1; i < numlevels; i++, j *= MAXNUMFILESINDIR)
    *numdirs += j;
    
  /* create the root directory */
  *dirs = (char **) myMalloc (sizeof(char *) * (*numdirs));
  (*dirs)[0] = (char *) myMalloc (20);
  sprintf((*dirs)[0], "l0%d.%d", count, getpid());
#ifdef DEBUG3
  printf("createFiles: mkdir\n");
#endif
  if (mkdir((*dirs)[0], S_IRWXU) < 0) {
    perror("createFiles, mkdir1");
    exit(1);
  }
#ifdef DEBUG2
  printf("createFiles: %s\n", (*dirs)[0]);
#endif
  
  /* loop, create the directory hierarchy, and save all of the
     names of the directories.  start index and end index for
     naming directories within a directory */
  startindex = 0;
  endindex = 1;
  temp = MAXNUMFILESINDIR;
  index = 1;
  for (i = 1; i < numlevels; i++) {
    for (j = startindex; j < endindex; j++) {
      for (k = 0; k < MAXNUMFILESINDIR; k++) {
	(*dirs)[index] = (char *) myMalloc (strlen((*dirs)[j]) + 25);
	sprintf((*dirs)[index], "%s/l%d%d%d.%d", ((*dirs)[j]), i, j, k, 
		count);
#ifdef DEBUG3
	printf("createFiles: mkdir 1\n");
#endif
	if (mkdir((*dirs)[index], S_IRWXU) < 0) {
	  perror("createFiles, mkdir2: ");
	  exit(1);
	}
	index++;
      }
    }
    startindex = endindex;
    endindex = startindex + temp;
    temp *= MAXNUMFILESINDIR;
  }

  /* name all of the files, which live at the lowest level of the
     directory hierarchy */
  (*files) = (char **) myMalloc (sizeof(char *) * numFiles);
  index = 0;
  for (i = startindex; i < endindex && index < numFiles; i++) {
    for (j = 0; (j < MAXNUMFILESINDIR) && (index < numFiles); j++) {
      (*files)[index] = (char *) myMalloc(strlen((*dirs)[i]) + 15);
      sprintf((*files)[index], "%s/f%d.%d", (*dirs)[i], index, count);
      index++;
    }
  }
  count++;
}

void
createHier(int numFiles, char ***dname)
/* creates a hierarchy with numFiles distinct paths */
{
  static int count = 0;
  int i, j, k, numlevels;
  int sum, mult;
  int index, startindex, endindex, temp;
  int fanout;

#ifdef DEBUG2
  printf("createHier\n");
#endif
  numlevels = 10;

  /* create the root */
  (*dname) = (char **) myMalloc (numFiles * sizeof(char *));
  (*dname)[0] = (char *) myMalloc (10);
  sprintf((*dname)[0], "h0%d.%d", count, getpid());
#ifdef DEBUG3
  printf("createHier: mkdir\n");
#endif
  if (mkdir((*dname)[0], S_IRWXU) < 0) {
    perror("CreateHier, mkdir1");
    exit(1);
  }
#ifdef DEBUG2
  printf("createHier: %s\n", (*dname)[0]);
#endif
  
  startindex = 0;
  endindex = 1;
  index = 1;
  
  /* figure out how many levels to put in, and how many directories
     to put into each level */
  for (i = 1; ; i++) {
    sum = 0;
    mult = 1;
    for (j = 0; j < numlevels; j++, mult *= i)
      sum += mult;
    if (sum >= numFiles)
      break;
  }
  fanout = i;

  /* create the hierarchy */
  for (i = 1; (i < numlevels) && (index < numFiles); i++) {
    for (j = startindex; (j < endindex) && (index < numFiles); j++) {
      for (k = 0; (k < fanout) && (index < numFiles); k++) {
	(*dname)[index] = (char *) myMalloc (strlen((*dname)[j]) + 25);
	sprintf((*dname)[index], "%s/h%d%d%d.%d", (*dname)[j], i, j, k, 
		count);
#ifdef DEBUG3
	printf("createhier: mkdir2\n");
#endif
	if (mkdir((*dname)[index], S_IRWXU) < 0) {
	  perror("CreateHier, mkdir2");
	  exit(1);
	}
	index++;
      }
    }
    if (i < (numlevels - 1)) {
      temp = startindex;
      startindex = endindex;
      endindex = (startindex - temp) * MAXNUMFILESINDIR;
    }
  }
  count++;
}

#if 0
void
flushBuffer()
/* Flush buffer cache */
{
  static count = 0;
  char fname[20];
  int i, fd;
  char *buf = (char *) myMalloc(blockSize);
  int fileSize = bufCacheSize * 5 / blockSize;

#ifdef DEBUG2
  printf("flushBuffer\n");
#endif
  sprintf(fname, "flush%d.%d", getpid(), count++);

  /* create file */
#ifdef DEBUG3
  printf("flushBuffer, open\n");
#endif
  if ((fd = open(fname, O_CREAT | O_TRUNC | O_RDWR, S_IRWXU)) < 0) {
    perror("flushBuffer, open");
    exit(1);
  }

  /* write to the file */
  for (i = 0; i < fileSize; i++) {
#ifdef DEBUG3
    printf("flushBuffer: write\n");
#endif
    if (write(fd, buf, blockSize) < blockSize) {
      perror("flushBuffer, write");
      exit(1);
    }
  }

  /* read the file */
#ifdef DEBUG3
  printf("flushBuffer: lseek\n");
#endif
  lseek(fd, 0, SEEK_SET);
  for (i = 0; i < fileSize; i++) {
#ifdef DEBUG3
    printf("flushBuffer: read\n");
#endif
    if (read(fd, buf, blockSize) < blockSize) {
      perror("flushBuffer, read");
      exit(1);
    }
  }

  /* clean up -- delete the file */
  close(fd);
#ifdef DEBUG3
  printf("flushBuffer: unlink\n");
#endif
  if (unlink(fname) < 0) {
    perror("flushBuffer, unlink");
    exit(1);
  }
  free(buf);
}

void
flushInode()
/* Flush inode cache -- the hierarchy used has already been created 
   at the beginning of the benchmark */
{
  int i;
  struct stat buf;
 
#ifdef DEBUG2
  printf("flushInode\n");
#endif

  /* stat the files sequentially */
  for (i = 0; i < numFilesFlush; i++)
    if (stat(fnameFlush[i], &buf) < 0) {
      perror("flushInode, stat");
      exit(1);
    }

  /* stat the files in random order */
  createPerm(&addrArrayFlush, numFilesFlush, 1, numFilesFlush);
  for (i = 0; i < numFilesFlush; i++)
    if (stat(fnameFlush[addrArrayFlush[i]], &buf) < 0) {
      perror("flushInode, stat2");
      exit(1);
    }
}

void
flushDNLC()
/* Flush directory-name lookup cache -- the hierarchy used has already
   been created at the beginning of the benchmark */
{
  int i;
  struct stat buf;

#ifdef DEBUG2
  printf("flushDNLC\n");
#endif

  /* stat the dirs sequentially */
  for (i = 0; i < numFilesDNLC; i++)
    if (stat(fnameDNLC[i], &buf) == -1) {
      perror("flushDNLC, stat");
      exit(1);
    }

  /* stat the dirs in random order */
  createPerm(&addrArrayDNLC, numFilesDNLC, 1, numFilesDNLC);
  for (i = 0; i < numFilesDNLC; i++)
    if (stat(fnameDNLC[addrArrayDNLC[i]], &buf) == -1) {
      perror("flushDNLC, stat");
      exit(1);
    }
}

void
flushAll()
/* Flush all caches */
{
  flushBuffer();
  flushInode();
  flushDNLC();
}
#endif

double
getavg(double *numbers)
{
  int i;
  double totTime = 0;

  for (i = 0; i < reps; i++) {
    totTime += numbers[i];
  }
  return (totTime / (double) reps);
}

double
getStdDev(double *numbers)
{
  int i;
  double avg = getavg(numbers);
  double stdDev = 0;

  for (i = 0; i < reps; i++) {
    stdDev += (numbers[i] - avg) * (numbers[i] - avg);
  }
  stdDev /= ((double) reps);
  if (stdDev > 0.0) {
     stdDev = sqrt(stdDev);
  }

  return stdDev;
}
