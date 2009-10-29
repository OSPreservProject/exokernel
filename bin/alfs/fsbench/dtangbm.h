
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

/* generic header file, with the generic includes */

#ifndef DTANGBM_H
#define DTANGBM_H

#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>

/* constants */
#define KB 1024
#define MB (1024*1024)
#define DISKTRANSFER 65536
#define REPS 5
#define MAXNUMFILESINDIR 100
#define FILESIZE (20*MB)
#define NUMPROCESSES 5
#define NUMPERMUTATIONS 10

#define CREATESEQ 0
#define DELETESEQ 1
#define CREATESYNC 2
#define DELETERAND 3
#define CREATE2 4
#define DELETESEQSYNC 5
#define CREATE3 6
#define DELETERANDSYNC 7
#define MKDIRSEQ 8
#define RMDIRSEQ 9
#define MKDIRSYNC 10
#define RMDIRRAND 11
#define MKDIR2 12
#define RMDIRSEQSYNC 13
#define MKDIR3 14
#define RMDIRRANDSYNC 15
#define SYNC 16
#define STATFSEQ 17
#define STATFRAND 18
#define STATDSEQ 19
#define STATDRAND 20
#define NUMOPS 21

#define SPATIALSEQ 0
#define SPATIALRAND 1
#define LOGICAL 2
#define INODECREATE 3
#define INODEACCESS 4
#define DIRCREATE 5
#define NAMELOOKUP 6
#define METADATATIME 7
#define CACHEAGE 8
#define READAHEAD 9
#define NUMBM 10
#define NUMCONCURRENT 7

/* structs for return data from the various micro-benchmarks */
struct spa_data {
  double *writeAllocTime;
  double *readTime;
  double *writeTime;
};

struct log_data {
  int numFiles;
  int totalSize;
  double *openCloseTime;
  double *openCloseRandDirTime;
  double *openCloseRandTime;
  double *readTime;
  double *readRandDirTime;
  double *readRandTime;
  double *writeTime;
  double *writeRandDirTime;
  double *writeRandTime;
};

struct sub_data {
  double *time1;
  double *time2;
};

struct sub3_data {
  double *time1;
  double *time2;
  double *time3;
};

struct time_data {
  double *times[NUMOPS];
};

struct dnlc_data {
  int numFiles;
  double *seqTime;
  double *randDirTime;
  double *randLevelTime;
  double *randTime;
};

struct age_data {
  double *rewriteTime;
  double *rewriteSyncTime;
  double *syncTime;
  double *charTime;
  double *rewriteHalfTime;
  double *halfSyncTime;
  long totalTime;
  long totalBlock;
  int maxTime;
};

struct readahead_data {
  int totBlocks;
  double *totTime;
  double *runLength[3][50];
  int numBlocks[3][50];
};

#include "exo-adapt.h"

#endif
