
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

#define printf if (0) printf
#define kprintf if (0) printf
extern mode_t make_st_mode(int mode,int type);



/* Permission Checking 
   return 0 on success */
/* make sure we check the right id's here (like euid instead of uid..) */
int 
check_read_permission(struct nfs_fattr *fattr) {
  DPRINTF(CLUHELP_LEVEL,("checking read perm: file mode: %o uid: %d gid: %d\n",
	   fattr->mode,fattr->uid, fattr->gid));

  if (fattr->mode & S_IROTH) return(1);
  if (fattr->mode & S_IRUSR && fattr->uid == __current->uid) return(1);
  if (fattr->mode & S_IRGRP && fattr->gid == __current->egid) return(1);

  if (fattr->mode & S_IRGRP && __is_in_group (fattr->gid)) {
    return 1;
  } else {
    return 0;
  }
}

int 
check_write_permission(struct nfs_fattr *fattr) {
  
  DPRINTF(CLUHELP_LEVEL,("checking write perm: file mode: %o uid: %d gid: %d\n",
	   fattr->mode,fattr->uid, fattr->gid));
  
  if (fattr->mode & S_IWOTH) return(1);
  if (fattr->mode & S_IWUSR && fattr->uid == __current->uid) return(1);
  if (fattr->mode & S_IWGRP && fattr->gid == __current->egid) return(1);

  if (fattr->mode & S_IWGRP && __is_in_group (fattr->gid)) {
    return 1;
  } else {
    return 0;
  }
}

int 
check_execute_permission(struct nfs_fattr *fattr) {

  DPRINTF(CLUHELP_LEVEL,("checking execute perm: file mode: %o uid: %d gid: %d\n",
	   fattr->mode,fattr->uid, fattr->gid));

  if (fattr->mode & S_IXOTH) return(1);
  if (fattr->mode & S_IXUSR && fattr->uid == __current->uid) return(1);
  if (fattr->mode & S_IXGRP && fattr->gid == __current->egid) return(1);


  if (fattr->mode & S_IXGRP && __is_in_group (fattr->gid)) {
    return 1;
  } else {
    return 0;
  }
}

int 
nfs_open(struct file *dirp, struct file *filp, char *name, 
	 int flags, mode_t mode) {
    struct nfs_fh *dhandle,sfhandle;
    struct nfs_fattr temp_fattr;
    struct nfs_sattr temp_sattr;
    nfsc_p e;
//    nfsld_p nfsld = (nfsld_p)filp->data;
    int status;
    DPRINTF(CLU_LEVEL,("nfs_open: dirp: %08x, name: %p, flags: %d, mode: %d.\n",
		       (int)dirp,name,flags,mode));
    demand(dirp, bogus dirp);
    demand(filp, bogus filp);

    demand(0,write sharing not fixed);

    DPRINTF(CLU_LEVEL,("%d",(pr_filp(dirp,"dirp"),0)));

    dhandle = GETFHANDLE(dirp);

    printf("nfs_open_nocache: nfs_proc_lookup %s\n",name);
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
      
      if (temp_fattr.type == NFREG) /* we dont handle symlinks for now */
	{
	  if ((flags & O_TRUNC) && 
	      ((flags & O_WRONLY) || (flags & O_RDWR)) && 
	      (temp_fattr.size > 0)) {
	    temp_sattr.mode = temp_fattr.mode; 
	    temp_sattr.uid  = temp_fattr.uid;
	    temp_sattr.gid  = temp_fattr.gid;
	    temp_sattr.size = 0;
	    temp_sattr.atime.seconds = -1;
	    temp_sattr.atime.useconds = 0;
	    temp_sattr.mtime.seconds = -1;
	    temp_sattr.mtime.useconds = 0;
	    status = nfs_proc_setattr(
				      &sfhandle,
				      &temp_sattr,
				      &temp_fattr);
	    if (status != 0) {errno = status; return -1;}
	  }
      } else { 
	if (temp_fattr.type != NFDIR) {
	  /* we can read a directory this way */
	  errno = EISDIR ; return -1;
	}
      }
    } else {
      /* means the file does not exists */
PR;
      if (flags & O_CREAT)
	{		
	  temp_sattr.mode = mode;
	  temp_sattr.uid  = __current->euid;
	  temp_sattr.gid  = __current->egid;
	  temp_sattr.size = 0;
	  temp_sattr.atime.seconds = -1;
	  temp_sattr.atime.useconds = 0;
	  temp_sattr.mtime.seconds = -1;
	  temp_sattr.mtime.useconds = 0;
	  status = nfs_proc_create(
				   dhandle,
				   name,
				   &temp_sattr,
				   &sfhandle,
				   &temp_fattr);
PR;
	  if (status != 0)
	  {DPRINTF(CLU_LEVEL,
		   ("2nfs_open error status = %d name: %s\n",status,name));
	   errno = status ; return -1;}
	  DPRINTF(CLU_LEVEL,("#### Creation of %s (%d) successful\n",name,status));
      } else {
	  DPRINTF(CLU_LEVEL,
		  ("3nfs_open error status = %d name: %s\n",status,name));
	  errno = ENOENT; return -1;
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
//   nfsld->diroffset = 0;

   {
     e = nfsc_get(filp->f_dev, filp->f_ino);
     assert(e);
     GETNFSCE(filp) = e;
     nfsc_set_fhandle(e,sfhandle);
     /* timestamp != 0, means we grabbed an old entry */
     if (nfsc_get_ts_sec(e) || nfsc_get_ts_usec(e)) {
       struct timeval b;
       b.tv_sec = nfsc_get_mtime(e);
       b.tv_usec = nfsc_get_mtimensec(e) / 1000;
       nfs_fill_stat(&temp_fattr,e);
       
       printf("op: %s, old entry, mod times s: o%ld n%d  u: o%ld n%ld\n",name,
	      b.tv_sec,e->sb.st_mtime,b.tv_usec,e->sb.st_mtimensec);
       if (b.tv_sec != nfsc_get_mtime(e) ||
	   b.tv_usec != nfsc_get_mtimensec(e)/1000) {
	 kprintf("NFS_OPEN: mtime changed %ld != %d || %ld != %ld FLUSH\n",
	       b.tv_sec,nfsc_get_mtime(e),
	       b.tv_usec,nfsc_get_mtimensec(e));
	 kprintf("For file: %s, ino: %d\n",name, filp->f_ino);
	 nfs_flush_filp(filp);
       }
     } else {
       nfs_fill_stat(&temp_fattr,e);
     }

     nfsc_settimestamp(e);
   }
   
   PRSHORTFILP("** NFS_OPEN",filp,"\n");
   return 0;
}

int 
nfs_close0(struct file *filp, int fd) {
  /* equivalent to sync */
  nfsc_p e;
  demand(filp,nfs_release: bogus filp);

  e = GETNFSCE(filp);
  assert(e);
  nfsc_sync(e);
  PRSHORTFILP("** NFS_CLOSE0",filp,"\n");
  return 0;
}
DEF_ALIAS_FN (nfs_sync,nfs_close0);

int 
nfs_close(struct file *filp) {
  nfsc_p e;
  demand(filp,nfs_release: bogus filp);

  e = GETNFSCE(filp);
  assert(e);
#if 0
  if (e == GETNFSCE(__current->root)) {
    printf("NFS_CLOSE ROOT bypass filp %p e: %p\n",filp,e);
    kprintf("NFS_CLOSE ROOT bypass filp %p e: %p\n",filp,e);
    return 0;
  } else {
    printf("NFS_CLOSE filp %p e: %p\n",filp,e);
    kprintf("NFS_CLOSE filp %p e: %p\n",filp,e);
  }
#endif
  nfsc_put(e);
  PRSHORTFILP("** NFS_CLOSE",filp,"\n");
  return 0;
}






int 
nfs_lookup(struct file *dirp, const char *name, struct file *filp) {
  struct nfs_fh *dhandle, sfhandle;
  struct nfs_fattr temp_fattr;
  int status;

  printf("nfs_lookup_nocache: %s\n",name);
  DPRINTF(CLU_LEVEL,("nfs_lookup:\n"));
  demand(dirp, bogus from (dirp) filp);
  demand(filp, bogus to (filp) filp);
  
  /* hardwire terminal support!! */
  if (!strcmp(name,"dev") && (FILPEQ(dirp,__current->root))) {
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

  if (!(S_ISDIR(dirp->f_mode))) {errno = ENOTDIR; return -1;}
  
  dhandle = GETFHANDLE(dirp);
  
  printf("nfs_lookup: nfs_proc_lookup %s\n",name);
  status = nfs_proc_lookup(dhandle,
			   name,
			   &sfhandle,
			   &temp_fattr);
  if (status != 0) {
    /* error */
    errno = status;
    return -1;
  } else {
    filp->f_pos = 0;
    filp_refcount_init(filp);
    filp->f_owner = __current->uid;
    filp->op_type = NFS_TYPE;
    filp->f_mode = make_st_mode(temp_fattr.mode,temp_fattr.type);
    filp->f_dev = FHGETDEV(dhandle);
    filp->f_ino = temp_fattr.fileid;
    filp->f_flags = 0;
    {
      nfsc_p e;
      e = nfsc_get(filp->f_dev, filp->f_ino);
      assert(e);
      GETNFSCE(filp) = e;
      nfsc_set_fhandle(e,sfhandle);

      /* timestamp != 0, means we grabbed an old entry */
      if (e->timestamp.tv_sec || e->timestamp.tv_usec) {
	struct timeval b;
	b.tv_sec = e->sb.st_mtime;
	b.tv_usec = e->sb.st_mtimensec/1000;
	nfs_fill_stat(&temp_fattr,e);
	printf("lk %s: old entry, mtime o%ld n%d  o%ld n%ld\n",name,
	       b.tv_sec,e->sb.st_mtime,b.tv_usec,e->sb.st_mtimensec);
	if (b.tv_sec != e->sb.st_mtime ||
	    b.tv_usec != e->sb.st_mtimensec/1000) {
	  kprintf("NFS_LOOKUP: mtime changed %ld != %d || %ld != %ld FSH\n",
	       b.tv_sec,e->sb.st_mtime,
	       b.tv_usec,e->sb.st_mtimensec);
	 kprintf("For file: %s, ino: %d\n",name, filp->f_ino);
	 nfs_flush_filp(filp);
	}
      } else {
	nfs_fill_stat(&temp_fattr,e);
      }
      
      nfsc_settimestamp(e);
    }
    
    PRSHORTFILP("** NFS_LOOKUP",filp,"\n");
    return 0;
  }
  
}
//#undef printf
//#undef kprintf
int 
nfs_release_nocache(struct file *filp) {
  nfsc_p e;
  demand(filp,nfs_release: bogus filp);

  e = GETNFSCE(filp);
  assert(e);
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
  nfsc_put(e);
  PRSHORTFILP("** NFS_RELEASE",filp,"\n");
  return 0;
}

