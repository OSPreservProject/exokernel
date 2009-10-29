
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
#include <sys/stat.h>
#include <string.h> 
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <exos/debug.h>

#include "fd/proc.h"

#include "nfs_rpc.h"
#include "nfs_struct.h"
#include "nfs_proc.h"

#include <malloc.h>
#include "exos/vm-layout.h"
#include <xok/sysinfo.h>

#include <exos/process.h>

extern struct file_ops nfs_file_ops;

/* found in nfs_root.c */
extern char *__nfs_root_host;
extern char *__nfs_root_path;

extern char *__nfs_root_host2;
extern char *__nfs_root_path2;

extern int 
nfs_pass_all_ref(u_int k, int envid, int ExecOnlyOrNewpid);

struct nfs_shared_data * const nfs_shared_data = (struct nfs_shared_data *) NFS_SHARED_REGION;

/* this should only be called once */
int
nfs_init(void) {
  int status;
  START(fd_op[NFS_TYPE],init);
  
  status = nfs_cache_init();
  
  demand(status == 0, nfs_cache_init failed);
  DPRINTF(CLUHELP_LEVEL,("nfs_init\n"));
  START(fd_op[NFS_TYPE],shm_alloc);
  status = fd_shm_alloc(FD_SHM_OFFSET + NFS_TYPE,
			(sizeof(struct nfs_shared_data)),
			(char *)NFS_SHARED_REGION);
  
  StaticAssert((sizeof(struct nfs_shared_data)) <= NFS_SHARED_REGION_SZ);
  
  STOP(fd_op[NFS_TYPE],shm_alloc);
  
  if (status == -1) {
    demand(0, problems attaching shm);
    return -1;
  }
  
  START(fd_op[NFS_TYPE],init2);
  
  register_file_ops(&nfs_file_ops, NFS_TYPE);
  STOP(fd_op[NFS_TYPE],init2);
  
  START(fd_op[NFS_TYPE],get);
  
  if (status) {
    StaticAssert(sizeof(nfsld_t) <= FILE_DATA_SIZE);
    /* printf("Initializing nfs shared data structture\n"); */
    FD_ZERO(&(nfs_shared_data->inuse));
    
    
    nfsc_init(nfs_shared_data->entries,NFSATTRCACHESZ);
    nfs_shared_data->hint_e = nfs_shared_data->entries;
    nfs_shared_data->xid = 0;
    //printf("mounting root (%s:%s)...\n",__nfs_root_host,__nfs_root_path);
#if 0    
    status = nfs_mount_root(__nfs_root_host,__nfs_root_path);
    if (status != 0) {
      printf("failed (errno: %d)\n",errno);
      printf("falling back to: (%s:%s)...",__nfs_root_host2,__nfs_root_path2);
      status = nfs_mount_root(__nfs_root_host2,__nfs_root_path2);
      if (status != 0) {
	printf("failed also (errno: %d), this is bad\n",errno);
      } else {
	printf("Ok\n");
      }
    } else {
      //printf("Ok\n");
    }
#endif
  } else {
    /*       printf("This is not first process, just attaching memory\n"); */
  }
  STOP(fd_op[NFS_TYPE],get);
  
  /* should really do it per server, for now each process has ra and wb structures */
  
  STOP(fd_op[NFS_TYPE],init);
  
  OnFork(nfs_pass_all_ref);
  OnExec(nfs_pass_all_ref);

  return 0;
}
