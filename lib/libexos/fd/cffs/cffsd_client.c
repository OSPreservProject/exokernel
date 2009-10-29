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
#include <xok/sys_ucall.h>
#include <xok/mmu.h>

#include <exos/ipc.h>
#include <exos/ipcdemux.h>
#include <exos/cffs_prot.h>
#include <exos/cap.h>

#include <assert.h>

/* client side stubs for ipc'ing to the cffsd server */

/* Currently this exports an interface that makes it look like
   the cffsd takes user pointers, but in reality cffsd takes 
   <dev, block, offset> triples into the buffer cache. So, we have
   to translate all address with respect to this address space into
   bc entries and look up the correct device, block and offset. */

/* verify ptr points into a page that is in the bc and then
   fill in the dev and blk that this ptr refers to */

/* XXX -- this should be static */

void vptr_to_bc_ref (void *ptr, struct bc_ref *r) {
  int ret;
  u_int ppn;

  ppn = va2ppn (ptr);
  assert (ppn < __sysinfo.si_nppages);
  
  ret = sys_bc_ppn2buf (ppn, &r->br_dev, &r->br_blk);
  assert (ret >= 0);

  r->br_off = (u_int )ptr % NBPG;
}

int cffsd_fsupdate_superblock (int action, 
			       struct superblock *superblock, 
			       u_int val1,
			       u_int val2) {

  struct cffsd_fsupdate_superblock_arg arg;
  int ret;
  int ipc_ret;

  vptr_to_bc_ref ((void *)superblock, &arg.superblock);
  arg.param1 = val1;
  arg.param2 = val2;

  ipc_ret = _ipcout_default (cffsd_get_eid (), 4, &ret, IPC_CFFSD, 
			     CFFSD_FSUPDATE_SUPERBLOCK, action, (u_int)&arg);
  assert (ipc_ret == IPC_RET_OK);
  assert (ret == 0);
  return ret;
}

int cffsd_fsupdate_dinode (int action,
			    struct dinode *dinode,
			    u_int val1,
			    u_int val2,
			    u_int val3) {

  struct cffsd_fsupdate_dinode_arg arg;
  int ret;
  int ipc_ret;

  vptr_to_bc_ref ((void *)dinode, &arg.dinode);
  arg.param1 = val1;
  arg.param2 = val2;
  arg.param3 = val3;

  ipc_ret = _ipcout_default (cffsd_get_eid (), 4, &ret, IPC_CFFSD, 
			     CFFSD_FSUPDATE_DINODE, action, (u_int)&arg);
  assert (ipc_ret == IPC_RET_OK);
  return ret;
}  

int cffsd_fsupdate_directory (int action,
			      struct embdirent *dirent,
			      u_int val2,
			      u_int val3,
			      u_int val4,
			      u_int val5) {

  struct cffsd_fsupdate_directory_arg arg;
  int ret;
  int ipc_ret;

  vptr_to_bc_ref ((void *)dirent, &arg.dirent);
  arg.param2 = val2;
  arg.uint_param3 = val3;
  arg.uint_param4 = val4;
  arg.param5 = val5;

  ipc_ret = _ipcout_default (cffsd_get_eid (), 4, &ret, IPC_CFFSD, 
			     CFFSD_FSUPDATE_DIRECTORY, action, (u_int)&arg);
  assert (ipc_ret == IPC_RET_OK);
  if (action != CFFS_DIRECTORY_SPLITENTRY) {
    assert (ret == 0);
  }

  return ret;
}  

int cffsd_fsupdate_directory3 (int action,
			       struct embdirent *dirent,
			       u_int val2,
			       u_int val3,
			       u_int val4,
			       u_int val5) {

  struct cffsd_fsupdate_directory_arg arg;
  struct dinode *dinode = (struct dinode *) val3;
  int ret;
  int ipc_ret;

  vptr_to_bc_ref ((void *)dirent, &arg.dirent);
  arg.param2 = val2;
  vptr_to_bc_ref ((void *)dinode, &arg.ref_param3);
  arg.uint_param4 = val4;
  arg.param5 = val5;

  ipc_ret = _ipcout_default (cffsd_get_eid (), 4, &ret, IPC_CFFSD, 
			     CFFSD_FSUPDATE_DIRECTORY, action, (u_int)&arg);
  assert (ipc_ret == IPC_RET_OK);
  assert (ret == 0);

  return ret;
}  

int cffsd_fsupdate_directory4 (int action,
			       struct embdirent *dirent,
			       u_int val2,
			       u_int val3,
			       u_int val4,
			       u_int val5) {

  struct dinode *dinode = (struct dinode *)val4;
  struct cffsd_fsupdate_directory_arg arg;
  int ret;
  int ipc_ret;

  vptr_to_bc_ref ((void *)dirent, &arg.dirent);
  arg.param2 = val2;
  arg.uint_param3 = val3;
  vptr_to_bc_ref ((void *)dinode, &arg.ref_param4);
  arg.param5 = val5;

  ipc_ret = _ipcout_default (cffsd_get_eid (), 4, &ret, IPC_CFFSD, 
			     CFFSD_FSUPDATE_DIRECTORY, action, (u_int)&arg);
  assert (ipc_ret == IPC_RET_OK);
  assert (ret == 0);

  return ret;
}  

int cffsd_fsupdate_directory34 (int action,
				struct embdirent *dirent,
				u_int val2,
				u_int val3,
				u_int val4,
				u_int val5) {

  struct dinode *dinode3 = (struct dinode *)val3;
  struct dinode *dinode4 = (struct dinode *)val4;
  struct cffsd_fsupdate_directory_arg arg;
  int ret;
  int ipc_ret;

  vptr_to_bc_ref ((void *)dirent, &arg.dirent);
  arg.param2 = val2;
  vptr_to_bc_ref ((void *)dinode3, &arg.ref_param3);
  vptr_to_bc_ref ((void *)dinode4, &arg.ref_param4);
  arg.param5 = val5;

  ipc_ret = _ipcout_default (cffsd_get_eid (), 4, &ret, IPC_CFFSD, 
			     CFFSD_FSUPDATE_DIRECTORY, action, (u_int)&arg);
  assert (ipc_ret == IPC_RET_OK);
  assert (ret == 0);
  
  return ret;
}  

int cffsd_fsupdate_setptr (int action, 
			   void *sb,
			   struct dinode *dinode, 
			   char *allocmap, 
			   u_int *ptrloc, 
			   u_int val) {

  struct cffsd_fsupdate_setptr_arg arg;
  int ret;
  int ipc_ret;
  
  vptr_to_bc_ref ((void *)sb, &arg.sb);
  if (dinode)
    vptr_to_bc_ref ((void *)dinode, &arg.dinode);
  if (allocmap)
    vptr_to_bc_ref ((void *)allocmap, &arg.allocmap);
  if (ptrloc)
    vptr_to_bc_ref ((void *)ptrloc, &arg.ptrloc);
  arg.val = val;

  ipc_ret = _ipcout_default (cffsd_get_eid (), 4, &ret, IPC_CFFSD,
			     CFFSD_FSUPDATE_SETPTR, action, (u_int)&arg);
  assert (ipc_ret == IPC_RET_OK);
  if (action != CFFS_SETPTR_EXTERNALALLOC && 
      action != CFFS_SETPTR_EXTERNALFREE &&
      action != CFFS_SETPTR_ALLOCATE) {
    assert (ret == 0);
  }

  return ret;
}
			
int cffsd_fsupdate_renamedir (int action,
			      struct embdirent *to,
			      struct embdirent *from,
			      struct dinode *dinode,
			      struct dinode *parentTo,
			      struct dinode *parentFrom,
			      struct dinode *dying) {

  assert (0 && "Not Implemented yet");
  return 0;
}
     
int cffsd_fsupdate_initAllocMap (char *map[],
				 u_int count,
				 u_int deadcnt,
				 u_int part) {
  struct cffsd_fsupdate_initAllocMap_arg arg;
  int ret;
  int ipc_ret;
  int i;

  for (i=0; i < count; i++) {
    vptr_to_bc_ref ((void *)map[i], &arg.allocmap[i]);
  }
  arg.count = count;
  arg.deadcnt = deadcnt;
  arg.part = part;

  ipc_ret = _ipcout_default (cffsd_get_eid (), 4, &ret, IPC_CFFSD, 
			     CFFSD_FSUPDATE_INITALLOCMAP, 0, (u_int)&arg);
  assert (ipc_ret == IPC_RET_OK);
  assert (ret == 0);

  return ret;
}

int cffsd_fsupdate_allocMicropart (char *map[], u_int count, u_int part, u_int micropart) {
  struct cffsd_fsupdate_allocMicropart_arg arg;
  int ret;
  int ipc_ret;
  int i;
  
  assert (count < CFFSD_MAX_ALLOCMICROPART_SZ);

  for (i = 0; i < count; i++) {
    vptr_to_bc_ref ((void *)map[i], &arg.allocmap[i]);
  }
  arg.count = count;
  arg.part = part;
  arg.micropart = micropart;

  ipc_ret = _ipcout_default (cffsd_get_eid (), 4, &ret, IPC_CFFSD,
			     CFFSD_FSUPDATE_ALLOCMICROPART, 0, (u_int) &arg);
  assert (ipc_ret == IPC_RET_OK);

  return ret;
}

int cffsd_boot (u_int part) {
  int ipc_ret;
  int ret;

  ipc_ret = _ipcout_default (cffsd_get_eid (), 4, &ret, IPC_CFFSD,
			     CFFSD_BOOT, part, 0 /* dummy arg */);
  assert (ipc_ret == IPC_RET_OK);

  return ret;
}

