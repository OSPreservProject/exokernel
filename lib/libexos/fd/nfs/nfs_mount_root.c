
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

#include <unistd.h>
				/* #include <memory.h> */
#include <stdlib.h>
#include <string.h>
#include "fd/proc.h"

#include "nfs_rpc.h"
#include "nfs_mnt.h"
#include "nfs_pmap.h"

#include "errno.h"
#include <exos/debug.h>
#include "fd/path.h"

extern mode_t
make_st_mode(int mode,int type);

int
nfs_getmount(char *hostname, char *path, struct file *filp) {
    int new_port,status;
    struct generic_server *temp_server;
    struct nfs_fattr temp_fattr;
    struct nfs_fh sfhandle;
    nfsc_p e;

    if ((filp == (struct file *)0) || 
	(path == (char *)0) || (*path == 0) ||
	(hostname == (char *)0) || (*hostname == 0)) {
	errno = EFAULT; 
	return -1;
    }

    temp_server = make_server(hostname,111);
    
    if (temp_server == NULL) {
      kprintf("warning make_server failed in nfs_getmount, errno: %d\n",errno);
      errno = ENOENT; return -1;}
/*
    pr_server(temp_server);
*/

    new_port = pmap_proc_getmap(temp_server,MNT_PROGRAM,
				MNT_VERSION,RPCUDP);
    if (new_port <= 0) {
      	free_server(temp_server);
	printf("error: mount port <= 0\n");
	return -1;
    }
    printf("mounting %s:%s, mountd on port %d...\n",hostname,path,new_port);
    change_server_port(temp_server,new_port);
    
    sfhandle.server = temp_server;
        
    status = mnt_proc_mnt(temp_server, path, 
			  &sfhandle);
    if (status != 0) {	free_server(temp_server);
	errno = status;
	return -1;
    }
    
    change_server_port(temp_server,NFS_PORT);
    
    status = nfs_proc_getattr(&sfhandle,
			      &temp_fattr);
    
    if (status != 0) {
	printf("nfs_getmount, was not able to do getattr\n"
	       "on the NFS filp\n");
	free_server(temp_server);
	errno = status;
	return -1;
    }
    
    filp->f_mode = make_st_mode(temp_fattr.mode,temp_fattr.type);
    
    if (!(S_ISDIR(filp->f_mode))) {
	free_server(temp_server);
	errno = ENOTDIR;
	return -1;
    }
    clear_filp_lock(filp);
    filp->f_pos = 0;
    filp->f_flags = O_DONOTRELEASE;
    filp_refcount_init(filp);
    filp->f_owner = __current->uid;
    filp->op_type = NFS_TYPE;
    filp->f_dev = FHGETDEV(&sfhandle);
    filp->f_ino = temp_fattr.fileid;

    e = nfsc_get(filp->f_dev, filp->f_ino);
    assert(e);
    GETNFSCE(filp) = e;
    nfsc_set_fhandle(e,sfhandle);
    nfsc_or_flags(e,NFSCE_NEVERRELEASE);

    nfs_fill_stat(&temp_fattr,e);
    nfsc_settimestamp(e);
    DPRINTF(CLU_LEVEL,("Mount Status: %d Mount Port: %d\n",
		       status,new_port));
    return 0;
}

int 
nfs_putmount(struct file *filp) {
    struct nfs_fh *fhandle;
    fhandle = GETFHANDLE(filp);
    free_server(fhandle->server);
    return 0;
}

int 
nfs_mount_root(char *hostname, char *path) {
    struct file *filp;
    int status;

    filp = getfilp();
    if (filp == NULL) {
      fprintf(stderr,"nfs_mount_root: getfilp failed, no more filps\n");
      return -1;
    }
    status = nfs_getmount(hostname,path,filp);

    if (status != 0) {
	putfilp(filp);
	return status;
    }

    if (__current->root != 0) {
	filp_refcount_dec(__current->root);
    }
    __current->root = filp;
    filp_refcount_inc(filp);

    if (__current->cwd != 0) {
	filp_refcount_dec(__current->cwd);
    }
    __current->cwd = filp;
    __current->cwd_isfd = 0;
    filp_refcount_inc(filp);


    return 0;
    
}

