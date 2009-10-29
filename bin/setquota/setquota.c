
/*
 * Copyright (C) 1998 Exotec, Inc.
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
 * associated documentation will at all times remain with Exotec, Inc..
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by Exotec, Inc. The rest
 * of this file is covered by the copyright notices, if any, listed below.
 */

#include <xok/sysinfo.h>

#include "cffs.h"
#include "cffs_proc.h"

#include <stdlib.h>
#include <stdio.h>

extern char *__progname;

void usage () {
  printf ("usage: %s dev sprblk new-quota\n", __progname);
  exit (-1);
}

void main (int argc, char *argv[]) {
  int dev, sprblk, quota;
  cffs_t *sb;
  buffer_t *sb_buf;
  int ret;

  if (argc != 4)
    usage ();

  dev = atoi (argv[1]);
  sprblk = atoi (argv[2]);
  quota = atoi (argv[3]);

  if (dev < 0 || dev >= __sysinfo.si_ndisks) {
    printf ("error\n");
    printf ("%s: invalid device (%d)\n", __progname, dev);
    exit (-1);
  }

  CFFS_CHECK_SETUP (dev);

  sb_buf = cffs_buffer_getBlock (dev, 0, 1, sprblk, 0, NULL);
  if (!sb_buf) {
    printf ("error\n");
    printf ("%s: could not read superblock (%d)\n", __progname, sprblk);
    exit (-1);
  }

  sb = (cffs_t *)(sb_buf->buffer);

  if (quota > sb->numblocks) {
    printf ("error\n");
    printf ("%s: quota (%d) greater than number of blocks in filesystem (%d)\n", __progname, quota, sb->numblocks);
    exit (-1);
  }

  ret = cffs_superblock_setQuota (sb_buf, quota);
  if (ret != 0) {
    printf ("error\n");
    printf ("%s: unknown error (%d) from cffs_superblock_setQuota\n", __progname, ret);
    exit (-1);
  }
      
  cffs_buffer_releaseBlock (sb_buf, BUFFER_WRITE);

  printf ("ok\n");
  exit (0);
}

