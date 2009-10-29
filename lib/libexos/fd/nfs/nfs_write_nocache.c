
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
nfs_write(struct file *filp, char *buffer, int nbyte, int blocking) {
  struct nfs_fattr fattr;
  struct nfs_fh *fhandle;
  nfsc_p e;
  size_t size;
  int count = 0, target, total = 0 ;
  int reading;
  int wsize;

  demand(filp, bogus filp);

  e = GETNFSCE(filp);
  size = nfsc_get_size(e);

  DPRINTF(CLU_LEVEL,
	  ("nfs_write: filp: %08x size: %d offset: %qd nbyte: %d\n",
	   (int)filp, size, filp->f_pos, nbyte));

  fhandle = GETFHANDLE(filp);

  wsize = fhandle->server->wsize * MAX_P_RPC;


  if ((filp->f_flags & O_WRONLY) || (filp->f_flags & O_RDWR))
    /* should also check is not a directory */
    {
      if (filp->f_flags & O_APPEND)
	filp->f_pos = size;

	if (nbyte == 0) return 0;

      target = nbyte;
      while (1)
	{
	  reading = (nbyte > wsize) ? wsize : nbyte;
	  count = nfs_proc_write(
				fhandle,
				filp->f_pos,
				reading,
				buffer,
				&fattr);
	  DPRINTF(CLU_LEVEL,("WRITING %d got %d\n",reading,count));
	  /* nfs_proc_write returns a 0 for a successful write */
	  if (count == 0) {
	      nbyte = nbyte - reading;
	      total += reading;
	      buffer += reading;
	      filp->f_pos += reading;
#ifdef NO_WRITE_SHARING
	      nfs_fill_stat(&fattr,e); /* we use the returned attributes */
	      nfsc_settimestamp(e);
#else
	      nfsc_ts_zero(e);	/* we dont know what could of 
				   happened before we flush the attributes */
#endif
	    }


	  if (nbyte == 0 || count != 0)
	    {
	      
	      if (count == 0)  {
		return total;
	      } else {
		errno = count;
		return -1;
	      }
	    }
	}
    } else {
	fprintf(stderr,"fd not opened for writing flags %08x (filp: %08x)!!\n",filp->f_flags,(int)filp);
	errno = EBADF; return -1;
    }
}
