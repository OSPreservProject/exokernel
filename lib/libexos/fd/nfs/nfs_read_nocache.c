
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

/* #define PRINTF_LEVEL 99 */
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h> 
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include "nfs_rpc.h"
#include "nfs_rpc_procs.h"
#include "nfs_struct.h"
#include "nfs_proc.h"

#include "errno.h"
#include <exos/debug.h>

#undef PR
#if 0
#define PR fprintf(stderr,"@ %s:%d\n",__FILE__,__LINE__)
#else
#define PR
#endif


int
nfs_read(struct file *filp, char *buffer, int nbyte, int blocking) {
  struct nfs_fattr fattr;
  struct nfs_fh *fhandle;
  int count = 0, target, total = 0 ;
  int reading;
  int rsize;

  demand(filp, bogus filp);
  DPRINTF(CLU_LEVEL,
	  ("nfs_read: filp: %08x  offset: %qd nbyte: %d\n",
	   (int)filp, filp->f_pos, nbyte));

  /* check to see if it is a directory */
  if (S_ISDIR(filp->f_mode)) {errno = EPERM; return -1;}
  /* make sure we have read permission */
  if ((filp->f_flags & O_WRONLY)) {errno = EBADF; return -1;}

  fhandle = GETFHANDLE(filp);

  rsize = FHGETRSIZE(fhandle) * MAX_P_RPC;

  DPRINTF(CLU_LEVEL,("%d",(pr_filp(filp,"READ FILP"),0))); 

  /* should also check is not a directory */

  if (nbyte == 0) return 0;
  target = nbyte;
  for (;;) {
    reading = (nbyte > rsize) ? rsize : nbyte;
    count = nfs_proc_read(fhandle,
			  filp->f_pos,
			  reading,
			  buffer,
			  &fattr);
    
    if (count > 0) {
      nbyte = nbyte - count;
      total += count;
      buffer += count;
      filp->f_pos += count;
    } else {
      if (count == 0) return(total);
      else return(count);
    }   
    DPRINTF(CLU_LEVEL,
	    ("target: %d total %d count %d reading %d %08x\n",
	     target, total, count, reading, (int)buffer));
    
    if (total == target || count != reading) {
      return(total);}
  }
}

