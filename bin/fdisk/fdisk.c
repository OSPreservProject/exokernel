
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

#include <xok/partition.h>
#include <xok/sys_ucall.h>
#include <xok/sysinfo.h>

#include <exos/cap.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void usage () {
  printf ("usage: fdisk [-p <dev> | -e <dev> <partition> <start> <size> <type> <boot>]\n");
  printf ("\t-p prints the partition table on a device\n");
  printf ("\t-e edits the specified entry (numbered startin at zero, boot=0|1)\n");
  exit (-1);
}

char mbr[512];
volatile int resptr;

void read_mbr (int dev) {
  int ret;

  mbr[0] = 1;
  resptr = 1;
  ret = sys_disk_mbr (0, dev, CAP_ROOT, mbr, (int *)&resptr);
  if (ret != 0) {
    printf ("fdisk: could not read mbr (ret = %d)\n", ret);
    exit (-1);
  }
  while (resptr);
}

void write_mbr (int dev) {
  int ret;

  resptr = 1;
  ret = sys_disk_mbr (1, dev, CAP_ROOT, mbr, (int *)&resptr);
  if (ret != 0) {
    printf ("fdisk: could not write mbr (ret = %d)\n", ret);
    exit (-1);
  }
  while (resptr);
}

void do_print (int argc, char *argv[]) {
  int dev;
  int i;
  struct Partition *p;

  if (argc != 2)
    usage ();

  if (sscanf (argv[1], "%d", &dev) != 1)
    usage ();
  
  if (dev < 0 || dev >= __sysinfo.si_ndisks)
    usage ();

  read_mbr (dev);
  
  printf ("%s\t%8s\t%8s\t%8s\t%s\t%s\n\n",
	  "Num",
	  "Start",
	  "End",
	  "Size",
	  "Type",
	  "Bootable");
  for (i = 0, p = (struct Partition *)&mbr[PAR_TBL_START]; i < 4; i++, p++) {
    printf ("%d\t%8d\t%8d\t%8d\t%d\t%d\n",
	    i,
	    p->par_first,
	    p->par_first+p->par_size,
	    p->par_size,
	    p->par_id,
	    p->par_boot ? 1 : 0);
  }
}  

void do_edit (int argc, char *argv[]) {
  int dev;
  int partition;
  int type;
  int boot;
  int start;
  int size;
  struct Partition *p;

  if (argc != 7)
    usage ();

  if (sscanf (argv[1], "%d", &dev) != 1)
    usage ();
  if (sscanf (argv[2], "%d", &partition) != 1)
    usage ();
  if (sscanf (argv[3], "%d", &start) != 1)
    usage ();
  if (sscanf (argv[4], "%d", &size) != 1)
    usage ();
  if (sscanf (argv[5], "%d", &type) != 1)
    usage ();
  if (sscanf (argv[6], "%d", &boot) != 1)
    usage ();

  if (boot != 0 && boot != 1)
    usage ();

  if (partition < 0 || partition > 3)
    usage ();
  
  if (dev < 0 || dev >= __sysinfo.si_ndisks)
    usage ();

  /* XXX -- should check upper bound on size... */
  if (start < 0)
    usage ();

  if (size < 0)
    usage ();

  read_mbr (dev);

  p = (struct Partition *)&mbr[PAR_TBL_START];
  p += partition;

  p->par_first = start;
  p->par_size = size;
  p->par_id = type;
  p->par_boot = boot;

  write_mbr (dev);
}

int main (int argc, char *argv[]) {
  if (argc < 2)
    usage ();

  if (!strcmp (argv[1], "-p"))
    do_print (--argc, ++argv);
  else if (!strcmp (argv[1], "-e"))
    do_edit (--argc, ++argv);
  else
    usage ();

  exit (0);
}

