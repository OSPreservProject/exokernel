
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

#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>


#include "nfs_rpc.h"
#include "nfs_rpc_procs.h"
#include "nfs_xdr.h"
#include "nfs_mnt.h"
#include "errno.h"
#include <exos/debug.h>

static inline int *
mnt_rpc_header(int *p, int procedure, int ruid) {
  return generic_rpc_header(p,MNT_PROGRAM,MNT_VERSION,procedure,ruid);
}

int 
mnt_proc_mnt(struct generic_server *server, char *directory, 
	     struct nfs_fh *fhandle) {
  
  int *p, *p0;
  int status;

  DPRINTF(CLUHELP2_LEVEL,("MNT call MNT\n"));
  if (!(p0 = overhead_rpc_alloc(server->rsize)))
    return -EIO;

  p = mnt_rpc_header(p0,MNTPROC_MNT,0);
  p = xdr_encode_string(p,directory);
  if ((status = generic_rpc_call(server, p0, p)) < 0) {
    overhead_rpc_free(p0);
    return status;
  }
  if (!(p = generic_rpc_verify(p0)))
    {status = -1;}
  else {
    status = ntohl(*p++);
	if (status == 0)
	  {
	    xdr_decode_fhandle(p, fhandle);
	    fhandle->server = server;
	    DPRINTF(CLUHELP2_LEVEL,("MNTPROC_MNT call good\n"));
	  }
      }
  DPRINTF(CLUHELP2_LEVEL,("pmap proc getmap status: %d\n",status));
  overhead_rpc_free(p0);
  return(status);
}


int 
mnt_proc_umntall(struct generic_server *server) {
  
  int *p, *p0;
  int status;

  DPRINTF(CLUHELP2_LEVEL,("MNT call UMNTALL\n"));
  if (!(p0 = overhead_rpc_alloc(server->rsize)))
    return -EIO;

  p = mnt_rpc_header(p0,MNTPROC_UMNTALL,0);
  if ((status = generic_rpc_call(server, p0, p)) < 0) {
    overhead_rpc_free(p0);
    return status;
  }
  if (!(p = generic_rpc_verify(p0)))
    {status = 0;}
  else {
    status = 1;
    DPRINTF(CLUHELP2_LEVEL,("MNTPROC_UMNTALL good\n"));
  }
  overhead_rpc_free(p0);
  return(status);
}


int 
mnt_proc_umnt(struct generic_server *server, const char *dirname) {
  int *p, *p0;
  int status;

  DPRINTF(CLUHELP2_LEVEL,("MNT call UMNT\n"));
  if (!(p0 = overhead_rpc_alloc(server->rsize)))
    return -EIO;

  p = mnt_rpc_header(p0,MNTPROC_UMNT,0);
  p = xdr_encode_string(p,dirname);
  if ((status = generic_rpc_call(server, p0, p)) < 0) {
    overhead_rpc_free(p0);
    return status;
  }
  if (!(p = generic_rpc_verify(p0)))
    {status = 0;}
  else {
    status = 1;
    DPRINTF(CLUHELP2_LEVEL,("MNTPROC_UMNT good\n"));
  }
  overhead_rpc_free(p0);
  return(status);
}

