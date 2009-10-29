
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

#undef PRINTF_LEVEL
//#define PRINTF_LEVEL 99
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

#include <fd/namei.h>
#include <exos/critical.h>

#undef PR
#if 0
#define PR fprintf(stderr,"@ %s:%d\n",__FILE__,__LINE__)
#else
#define PR
#endif

extern mode_t make_st_mode(int mode,int type);
extern int check_read_permission(struct nfs_fattr *fattr);
extern int check_write_permission(struct nfs_fattr *fattr);
extern int check_execute_permission(struct nfs_fattr *fattr);




#if 1
#define lprintf(format,args...) kprintf(format , ## args)
#else
#define lprintf(format,args...)
#endif

#if 0
#define rprintf(format,args...) printf(format , ## args)
#else
#define rprintf(format,args...)
#endif

#if 1
#define kprintf(format,args...) printf(format , ## args)
#else
#define kprintf(format,args...)
#endif

#if 0
#define printf(format,args...) printf(format , ## args)
#else
#define printf(format,args...)
#endif

#if 1
#define fprintf(out,format,args...) fprintf(out,format , ## args)
#else
#define fprintf(format,args...)
#endif


/* ----------------------------------------------------------------------------- */

static int __nlkc_did_getattr = 0;
int 
nfs_lookup_cached(struct file *dirp, const char *name, struct file *filp) {
  struct nfs_fh *dhandle, sfhandle;
  struct nfs_fh *fhandle;

  struct nfs_fattr temp_fattr0,*temp_fattr = &temp_fattr0 ;
  int status;
  int dev,ino;
  nfsc_p d,f;


  DPRINTF(CLU_LEVEL,("nfs_lookup_cached: %s\n",name));
  demand(dirp, bogus from (dirp) filp);
  demand(filp, bogus to (filp) filp);
  memset((char *)temp_fattr, 0, sizeof (struct nfs_fattr));
  
  /* hardwire terminal support!! */
  if ((FILPEQ(dirp,__current->root))) {
    if (!strcmp(name,"dev")) {
    filp->f_ino = 599999;
    filp->f_pos = 0;
    filp_refcount_init(filp);
    filp_refcount_inc(filp);
    filp->f_owner = __current->uid;
#ifdef NEWPTY
    filp->op_type = CDEV_TYPE;
#else
    filp->op_type = NPTY_TYPE;
#endif
    filp->f_mode = S_IFDIR | 0755;
    filp->f_flags = 0;
    return 0;
    }
    if (!strcmp(name,"oev")) {
      /*       fprintf(stderr,"entering terminal directory\n"); */
      filp->f_ino = 599998;
      filp->f_pos = 0;
      filp_refcount_init(filp);
      filp_refcount_inc(filp);
      filp->f_owner = __current->uid;
      filp->op_type = PTY_TYPE;
      filp->f_mode = S_IFDIR | 0755;
      filp->f_flags = 0;
      return 0;
    }
  }

  if (!(S_ISDIR(dirp->f_mode))) {errno = ENOTDIR; return -1;}

  printf("dipr: %p, dirp->data: %p dirp->data->e: %p \n",
	  dirp,((nfsld_p)((filp)->data)),(((nfsld_p)((filp)->data))->e));

  d = GETNFSCE(dirp);
  assert(d);
  dhandle = GETFHANDLE(dirp);
  assert(dhandle);

  /* lookup in cache */
  status = nfs_cache_lookup(FHGETDEV(dhandle),GETNFSCEINO(d),name,&f);
  switch(status) {
  retrymiss:
  case 0:
    /* cache miss */
    /* name cache miss. */
    kprintf("nfs_lookup_cached: nfs_proc_lookup %s\n",name);
    status = nfs_proc_lookup(dhandle,
			     name,
			     &sfhandle,
			     temp_fattr);




    if (status != 0) {
      /* error */
      if (status == ENOENT) {
	/* should make sure that the entry is properly invalidated
	 * somehow must keep track if the dirp mod time changes
	 */
	/* SEE NOTE 1 */
//	fprintf(stderr,"CACHEADD0 negative: %d %s, from within lookup\n",
//		GETNFSCEINO(d),name);
	nfs_cache_add(d,(char *)name,(nfsc_p)0);
      }
      errno = status;
      return -1;
    } else {
  
      filp->f_pos = 0;
      filp_refcount_init(filp);
      filp->f_owner = __current->uid;
      filp->op_type = NFS_TYPE;
      filp->f_mode = make_st_mode(temp_fattr->mode,temp_fattr->type);
      filp->f_dev = FHGETDEV(dhandle);
      filp->f_ino = temp_fattr->fileid;
      filp->f_flags = 0;

      f = nfsc_get(filp->f_dev, filp->f_ino);

  
      assert(f);
      GETNFSCE(filp) = f;
  
      nfsc_set_fhandle(f, sfhandle);
      
      {
	/* cache the time, then compare mod times. */
	if (nfsc_neq_mtime(f,temp_fattr) || nfsc_ts_iszero(f)) {
	  /* mod times have changed */
	  nfs_flush_filp(filp);
	}
      }
      nfs_fill_stat(temp_fattr,f);
      nfsc_settimestamp(f);
      
      nfs_cache_add(d,(char *)name,f);
      return 0;
    }
    break;
  case 1:
    /* cache hit */

    assert(f);

    fhandle = &f->fh;
    dev = GETNFSCEDEV(f);
    ino = GETNFSCEINO(f);

    status = nfsc_checkchange(f, NFSLOOKUPMIN);
    switch(status) {
    case 0:
       break;
    case 1:
      nfs_flush_nfsce(f);
      /* if a directory has changed, we remove all names cached for it */
      if (S_ISDIR(d->sb.st_mode)) {nfs_cache_remove(dirp,NULL);}
      break;
    case -1:
      if (errno = ESTALE) {
	nfs_cache_remove(dirp,name);
	nfsc_put(f);
//	fprintf(stderr,"RETRYING\n\n");
	goto retrymiss;
      }
      return status;
    }
    
    /* we have a value nfsce in the cache. */
    GETNFSCE(filp) = f;
    filp->f_pos = 0;
    filp_refcount_init(filp);
    filp->f_owner = __current->uid;
    filp->op_type = NFS_TYPE;
    filp->f_mode = f->sb.st_mode;
    filp->f_dev = dev;
    filp->f_ino = ino;
    filp->f_flags = 0;
  
    return 0;
    break;
  case -1:
    /* negative cache hit */
    if ((status = nfsc_checkchange(d,NFSLOOKUPMIN)) == 1) {
      /* ADDED these 3 LINES */
      nfs_flush_filp(dirp);
      /* if a directory has changed, we remove all names cached for it */
      if (S_ISDIR(d->sb.st_mode)) {nfs_cache_remove(dirp,NULL);}
      else nfs_cache_remove(dirp,name);
//      fprintf(stderr,"RETRYING\n\n");
      goto retrymiss;
    } else {
      errno = ENOENT;
      return -1;
    }
    break;
  }
  assert(0);
  return -1;
}
//#undef printf
//#undef kprintf

int 
nfs_acquire(struct file *filp) {
  nfsc_p e;
  demand(filp,nfs_release: bogus filp);
  e = GETNFSCE(filp);

  if (e == NULL) {
    printf("nfs_acquire, null nfsc\n"); pr_filp(filp,"nfs_acquire");
    assert(0);
  } else {
    EnterCritical();
    rprintf("NFS_ACQUIRE: (%d) %d, %x, before internal ref: %d \n",
	    NFSCENUM(e),filp->f_ino, filp->f_mode,
	    nfsc_get_refcnt(e));
    //    fprintf(stderr,"NFSC_GET request %d,%d (from nfs_acquire)\n",
		  //	    GETNFSCEDEV(e),GETNFSCEINO(e));
    nfsc_inc_refcnt(e);
    ExitCritical();
  }
  return 0;

}

int 
nfs_release(struct file *filp) {
  nfsc_p e;
  demand(filp,nfs_release: bogus filp);
  e = GETNFSCE(filp);

  if (e == NULL) {
    printf("nfs_release, null nfsc\n");
    pr_filp(filp,"nfs_release");
  } else {
    rprintf("NFS_RELEASE: (%d) %d, %x, %p before internal ref: %d\n",NFSCENUM(e),
	    filp->f_ino, filp->f_mode, filp, nfsc_get_refcnt(e));
    nfsc_put(e);
  }
#if 0
  if (__current->root->op_type == NFS_TYPE && e == GETNFSCE(__current->root)) {
    printf("NFS_RELEASE ROOT bypass filp %p e: %p\n",filp,e);
    kprintf("NFS_RELEASE ROOT bypass filp %p e: %p\n",filp,e);
    return 0;
  } else {
    printf("NFS_RELEASE filp %p e: %p\n",filp,e);
    kprintf("NFS_RELEASE filp %p e: %p\n",filp,e);
  }
#endif
  PRSHORTFILP("** NFS_RELEASE",filp,"\n");
  return 0;
}





int 
nfs_open_cached(struct file *dirp, struct file *filp, char *name, 
	 int flags, mode_t mode) {
    struct nfs_fh *dhandle,sfhandle;
    struct nfs_fattr temp_fattr;
    struct nfs_sattr temp_sattr;
    nfsc_p e,d;
    int status;

    DPRINTF(CLU_LEVEL,("nfs_open_cached: dirp: %08x, name: %s, flags: %d, mode: %d.\n",
		       (int)dirp,name,flags,mode));
    demand(dirp, bogus dirp);
    demand(filp, bogus filp);

    DPRINTF(CLU_LEVEL,("%d",(pr_filp(dirp,"dirp"),0)));

    dhandle = GETFHANDLE(dirp);



#if 1
    __nlkc_did_getattr = 0;
    status = nfs_lookup_cached(dirp, name, filp);

    if (status == 0) {
      //kprintf("file exists, flags: %x %x %x %x, mode: %x\n",
		//flags, O_TRUNC, O_EXCL, O_CREAT, filp->f_mode);
      if (!(flags & O_TRUNC) && !(flags & O_EXCL) 
	  && !(flags & O_CREAT) && !(flags & O_WRONLY) && !(flags & O_RDWR) &&
	  (S_ISDIR(filp->f_mode) || S_ISREG(filp->f_mode))) {
	/* make sure we refresh attributes at every open */
	if (!__nlkc_did_getattr) {
	  switch (nfsc_checkchange(GETNFSCE(filp), -1)) {
	  case 0:
	    break;
	  case 1:
	    nfs_flush_filp(filp);
	    break;
	  case -1:
	    nfs_release(filp);
	    goto normal_open;
	  }
	} 
	/* should check permissions here */
	return 0;
      } else {
	nfs_release(filp);
      }
    } else if (errno == ENOENT && !(flags & O_CREAT)) {
      return -1;
    }

    //kprintf("nfs_open_cached: nfs_proc_lookup %s\n",name);
	 normal_open:
#endif

    status = nfs_proc_lookup(dhandle,
			     name,
			     &sfhandle,
			     &temp_fattr);
  
  
    DPRINTF(CLU_LEVEL,("#### lookup: %d\n",status));
    if (status == 0) {	/* means the file exists */
      if ((flags & O_CREAT) && (flags & O_EXCL))
	  { errno = EEXIST; return -1;}
      
      if (!(flags & O_WRONLY))	/* check for read permission */
	if (!check_read_permission(&temp_fattr)) 
	   {errno = EACCES; return -1;}

      if ((flags & O_WRONLY) || (flags & O_RDWR))
	if (!check_write_permission(&temp_fattr)) 
	  {errno = EACCES; return -1;}
      
      /* we dont handle symlinks here, they are handle by generic open */
      if (temp_fattr.type == NFREG) {
	if ((flags & O_TRUNC) && 
	    ((flags & O_WRONLY) || (flags & O_RDWR)) && 
	    (temp_fattr.size > 0)) {
	  temp_sattr.mode = temp_fattr.mode; 
	  temp_sattr.uid  = temp_fattr.uid;
	  temp_sattr.gid  = temp_fattr.gid;
	  temp_sattr.size = 0;
	  temp_sattr.atime.seconds =  -1;
	  temp_sattr.atime.useconds = -1;
	  temp_sattr.mtime.seconds =  -1;
	  temp_sattr.mtime.useconds = -1;
	  status = nfs_proc_setattr(&sfhandle,
				    &temp_sattr,
				    &temp_fattr);
	  if (status != 0) {errno = status; return -1;}
	}
      } else { 
	if (temp_fattr.type == NFDIR) {
	  /* we can read a directory this way */
	  errno = EISDIR ; return -1;
	}
      }
    } else {
      /* means the file does not exists */
      if (flags & O_CREAT) {		
	temp_sattr.mode = mode;
	temp_sattr.uid  = __current->euid;
	temp_sattr.gid  = __current->egid;
	temp_sattr.size = 0;
	temp_sattr.atime.seconds =  -1;
	temp_sattr.atime.useconds = -1;
	temp_sattr.mtime.seconds =  -1;
	temp_sattr.mtime.useconds = -1;
	status = nfs_proc_create(dhandle,
				 name,
				 &temp_sattr,
				 &sfhandle,
				 &temp_fattr);
	if (status != 0) {
	  DPRINTF(CLU_LEVEL,
		  ("2nfs_open_cached error status = %d name: %s\n",status,name));
	  errno = status ; return -1;
	} else {
	  DPRINTF(CLU_LEVEL,("#### Creation of %s (%d) successful\n",name,status));
	  nfs_flush_filp(dirp);
	}
      } else {
	DPRINTF(CLU_LEVEL,
		("3nfs_open_cached error status = %d name: %s\n",status,name));
#if 0
	if (status == ENOENT) 
	  nfs_cache_add(GETNFSCE(dirp),(char *)name,(nfsc_p)0);
#endif
	errno = status; return -1;
      }
    }
    clear_filp_lock(filp);
    
    filp->f_mode = make_st_mode(temp_fattr.mode,temp_fattr.type);
    filp->f_pos = 0;
    filp->f_dev = FHGETDEV(dhandle);
    filp->f_ino = temp_fattr.fileid;
    filp->f_flags = flags;
    filp_refcount_init(filp);
    filp_refcount_inc(filp);
    filp->f_owner = __current->uid;
    filp->op_type = NFS_TYPE;
    
    e = nfsc_get(filp->f_dev, filp->f_ino);
    assert(e);
    GETNFSCE(filp) = e;
    nfsc_set_fhandle(e,sfhandle);
    /* timestamp != 0, means we grabbed an old entry */
    /* if (nfsc_get_ts_sec(e) || nfsc_get_ts_usec(e)) { */
    if (1) {
      struct timeval b;
      b.tv_sec = nfsc_get_mtime(e);
      b.tv_usec = nfsc_get_mtimensec(e) / 1000;
      nfs_fill_stat(&temp_fattr,e);
      
      printf("op: %s, old entry, mod times s: o%ld n%ld  u: o%ld n%ld\n",name,
	     b.tv_sec,nfsc_get_mtime(e),b.tv_usec,nfsc_get_mtimensec(e));
      if (b.tv_sec != nfsc_get_mtime(e) ||
	  b.tv_usec != nfsc_get_mtimensec(e)/1000 || nfsc_ts_iszero(e)) {
	kprintf("NFS_OPEN_CACHED: mtime changed %ld != %ld || %ld != %ld FLUSH\n",
		b.tv_sec,nfsc_get_mtime(e),
		b.tv_usec,nfsc_get_mtimensec(e));
	kprintf("For file: %s, ino: %d\n",name, filp->f_ino);
	nfs_flush_filp(filp);
      }
    } else {
      nfs_fill_stat(&temp_fattr,e);
    }
    
    nfsc_settimestamp(e);
    d = GETNFSCE(dirp);
    if (!EQDOT(name)) nfs_cache_add(d,name, e);
#if 0
    fprintf(stderr,"CACHEADD2 positive: %d %s, from within open\n",
	    GETNFSCEINO(d),name);
#endif
    
    PRSHORTFILP("** NFS_OPEN_CACHED",filp,"\n");
    return 0;
}

