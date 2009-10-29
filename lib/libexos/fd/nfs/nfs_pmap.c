
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
#include "netinet/in.h"
#include <string.h>

#include "nfs_rpc.h"
#include "nfs_pmap.h"
#include "nfs_rpc_procs.h"

#include "errno.h"
#include <exos/debug.h>

inline static int *pmap_rpc_header(int *p, int procedure, int ruid);


inline static int *
pmap_rpc_header(int *p, int procedure, int ruid) {
  return generic_rpc_header(p,PMAP_PROGRAM,PMAP_VERSION,procedure,ruid);
}

int 
pmap_proc_null(struct generic_server *server)
{
  int *p, *p0;
  int status;
  int ruid = 0;
  
  DPRINTF(CLUHELP2_LEVEL,("MNT call NULL\n"));
  if (!(p0 = overhead_rpc_alloc(server->rsize)))
    return -EIO;

  p = pmap_rpc_header(p0, PMAPPROC_NULL, ruid);
  if ((status = generic_rpc_call(server, p0, p)) < 0) {
    overhead_rpc_free(p0);
    return status;
  }
  if (!(p = generic_rpc_verify(p0)))
    status = NFSERR_IO;
  else if ((status = ntohl(*p++)) == NFS_OK) {
    DPRINTF(CLUHELP2_LEVEL,("NULL good\n"));
  }
  overhead_rpc_free(p0);
  return nfs_stat_to_errno(status);
}

int 
pmap_proc_getmap(struct generic_server *server, 
		int prog, int vers, int prot) {
  int *p, *p0;
  int status;
  
  DPRINTF(CLUHELP2_LEVEL,("PMAP call GETPORT\n"));
  if (!(p0 = overhead_rpc_alloc(server->rsize)))
    return -EIO;

  p = pmap_rpc_header(p0,PMAPPROC_GETPORT,0);
  *p++ = htonl(prog);
  *p++ = htonl(vers);
  *p++ = htonl(prot);
  *p++ = htonl(0);
  if ((status = generic_rpc_call(server, p0, p)) < 0) {
    overhead_rpc_free(p0);
    return status;
  }
  if (!(p = generic_rpc_verify(p0)))
    status = 0;
  else {status = ntohl(*p);}
  DPRINTF(CLUHELP2_LEVEL,("PMAP proc getmap status: %d\n",status));
  overhead_rpc_free(p0);
  return (status);
}









