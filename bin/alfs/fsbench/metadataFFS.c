
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

/* Micro-Benchmark: Meta-data Performance
   For Unix/FFS specific:
   1. Inode access time
   2. Inode create time
   3. Directory access time
   4. Directory create time

   For non-OS specific:
   Measure seek times for various meta-data operations: create, unlink,
   mkdir, rmdir, stat, rename

   This is the Unix/FFS specific part.
*/

#define NOHARDLINKS

#include "dtangbm.h"
#include "utility.h"

extern int inodeCacheSize;
extern int reps;

void
getInodeCreateTimes(struct sub3_data *inodeCreate, char **dname, char **fname,
		    int numdirs, int numFiles)
{
  char **flink;
  int fd;
  int i, j, index;
  int startTime, endTime;
  struct alfs_stat buf;

#ifdef DEBUG
  printf("InodeCreateTime\n");
#endif

  /* name files */
  flink = (char **) myMalloc (sizeof(char *) * numFiles);
  index = 0;
  i = numdirs - numFiles / MAXNUMFILESINDIR;
  if (numFiles % MAXNUMFILESINDIR != 0)
    i--;
  for (; i < numdirs && index < numFiles; i++) {
    for (j = 0; (j < MAXNUMFILESINDIR) && (index < numFiles); j++) {
      flink[index] = (char *) myMalloc(strlen(dname[i]) + 25);
      sprintf(flink[index], "%s/link%d.%d", dname[i], index, getpid());
      index++;
    }
  }

  /* loop _reps_ time to increase accuracy */
  for (j = 0; j < reps; j++) {
    /* flush caches */
    flushAll();

    /* stat the directories */
    for (i = 0; i < numdirs; i++)
      if (stat(dname[i], &buf) == -1) {
	perror("inodeCreate, dir stat");
	exit(1);
      }

    /* create the files */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if ((fd = open(fname[i], O_RDWR | O_CREAT | O_TRUNC, S_IRWXU)) == -1) {
	perror("inodeCreate, create");
	exit(1);
      }
      close(fd);
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    inodeCreate->time1[j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, time: %f\n", numFiles, inodeCreate->time1[j]);
#endif

    /* flush caches */
    flushAll();

    /* stat the files */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++)
      if (stat(fname[i], &buf) == -1) {
	perror("inodeCreate, file stat");
	exit(1);
      }
    gettimeofday(&endTime, (struct timezone *) NULL);
    inodeCreate->time2[j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles, %d, time: %f\n", numFiles, inodeCreate->time2[j]);
#endif

    /* flush caches */
    flushAll();

#ifndef NOHARDLINKS
    /* create the hard-links */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (stat(fname[i], &buf) == -1) {
	perror("inodeCreate, file stat 2");
	exit(1);
      }
      if (link(fname[i], flink[i]) == -1) {
	perror("inodeCreate, link");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    inodeCreate->time3[j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numLinks: %d, time: %f\n", numFiles, inodeCreate->time3[j]);
#endif
#endif /* NOHARDLINKS */

    /* remove everything */
    for (i = 0; i < numFiles; i++) {
#ifndef NOHARDLINKS
      if (unlink(flink[i]) < 0) {
	perror("inodeCreateTime, unlink link");
	exit(1);
      }
#endif
      if (unlink(fname[i]) < 0) {
	perror("inodeCreateTime, unlink");
	exit(1);
      }
    }
    sync();
  }

  for (i = 0; i < numFiles; i++)
    free(flink[i]);
  free(flink);
}

void
getInodeAccessTimes(struct sub_data *inodeAccess, char **dname, char **fname,
		    int numdirs, int numFiles)
{
  int *perm;
  int fd, i, j;
  int startTime, endTime;
  struct alfs_stat buf;

#ifdef DEBUG
  printf("InodeAccessTime\n");
#endif

  perm = (int *) myMalloc (sizeof(int) * numFiles);

  /* create the files */
  for (i = 0; i < numFiles; i++) {
    if ((fd = open(fname[i], O_RDWR | O_CREAT | O_TRUNC, S_IRWXU)) == -1) {
      perror("inodeAccess, create");
      exit(1);
    }
    close(fd);
  }
    
  for (j = 0; j < reps; j++) {
    /* create permuation */
    createPerm(&perm, numFiles, 1, numFiles);

    /* flush buffers */
    flushAll();
    
    /* stat the path to get the path into the cache */
    for (i = 0; i < numdirs; i++)
      if (stat(dname[i], &buf) < 0) {
	perror("inodeAccess, stat path");
	exit(1);
      }

    /* stat in creation order */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (stat(fname[i], &buf) < 0) {
	perror("inodeAccess, stat seq");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    inodeAccess->time1[j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("SEQ: numFiles: %d, time: %f\n", numFiles, inodeAccess->time1[j]);
#endif

    /* flush buffers */
    flushAll();

    /* stat the path to get the path into the cache */
    for (i = 0; i < numdirs; i++)
      if (stat(dname[i], &buf) < 0) {
	perror("inodeAccess, stat path 2");
	exit(1);
      }

    /* stat in random order */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (stat(fname[perm[i]], &buf) < 0) {
	perror("inodeAccess, stat rand");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    inodeAccess->time2[j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("RAND: numFiles: %d, time: %f\n", numFiles, inodeAccess->time2[j]);
#endif
  }

  /* clean-up */
  for (i = 0; i < numFiles; i++)
    if (unlink(fname[i]) < 0) {
      perror("inodeAccessTime, unlink");
      exit(1);
    }
  free(perm);
}

void
getDirCreateTimes(struct sub_data *dirCreate, char **dname, char **fname,
		  int numdirs, int numFiles)
{
  int fd, i, j;
  int startTime, endTime;

#ifdef DEBUG
  printf("DirCreateTime\n");
#endif

  for (j = 0; j < reps; j++) {
    /* flush caches */
    flushAll();

    /* create the files */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if ((fd = open(fname[i], O_RDWR | O_CREAT | O_TRUNC, S_IRWXU)) == -1) {
	perror("dirCreate, create");
	exit(1);
      }
      close(fd);
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    dirCreate->time1[j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numFiles: %d, time: %f\n", numFiles, dirCreate->time1[j]);
#endif
    
    /* remove the files */
    for (i = 0; i < numFiles; i++) {
      if (unlink(fname[i]) < 0) {
	perror("dirCreateTime, unlink");
	exit(1);
      }
    }
    sync();

    /* flush caches */
    flushAll();
    
    /* create the directories */
    gettimeofday(&startTime, (struct timezone *) NULL);
    for (i = 0; i < numFiles; i++) {
      if (mkdir(fname[i], S_IRWXU) == -1) {
	perror("dirCreate, mkdir");
	exit(1);
      }
    }
    gettimeofday(&endTime, (struct timezone *) NULL);
    dirCreate->time2[j] = diffTime(startTime, endTime);
#ifdef DEBUG
    printf("numDirs: %d, time: %f\n", numFiles, dirCreate->time2[j]);
#endif
    
    /* remove the files */
    for (i = 0; i < numFiles; i++) {
      if (rmdir(fname[i]) < 0) {
	perror("dirCreateTime, rmdir");
	exit(1);
      }
    }
    sync();
  }
}
