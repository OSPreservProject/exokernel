
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


#include "fd/cffs/cffs.h"

#include <stdio.h>
#include <stdlib.h>

void usage(char *name) {
      printf("Usage: %s <devname> [<size>| -M #>\n", name);
      exit(0);
}

int main (int argc, char **argv) {
   off_t sz;
   int ret;

   if (argc != 3 && argc != 4) usage(argv[0]);

   if (!strcmp(argv[2],"-M")) {
      if (argc != 4) usage(argv[0]);
      sz = atoi(argv[3]);
      sz *= 1024*1024;
   } else {
      sz = atoi(argv[2]);
   }

   //printf("Initializing to %qd bytes\n",sz);
   ret = cffs_initFS (argv[1], sz);

   if (ret == 0) {
      printf ("Disk #%d initialized with %qd-byte (max) C-FFS file system\n", atoi(argv[1]), sz);
   } else {
      printf ("** FAILED (%d) ** %qd-byte C-FFS fs could not be created on disk #%d\n", ret, sz, atoi(argv[1]));
   }
   exit (ret);
}

