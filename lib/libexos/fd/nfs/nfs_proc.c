
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

//#undef PRINTF_LEVEL
//#define PRINTF_LEVEL 99
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h> 
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <exos/mallocs.h>

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

#define k1printf if (0) kprintf
#define k2printf  if (0) printf
#define fprintf  if (0) fprintf


/* for removing nfs files while still referenced */
static void nfs_process_unlinkatend(void);
static void nfs_unlinkatend(char *name, struct nfs_fh *fhandle);


/* GROK -- I don't know *why* hb treats fsync the same as close0, but this */
/* will at least decouple their prototypes... */

int nfs_fsync (struct file *filp)
{
   return (nfs_close0 (filp, -1));
}


#undef PRSHORTFILP
#define PRSHORTFILP(a,b,c)


off_t 
nfs_lseek(struct file *filp, off_t offset, int whence)
{
  off_t tmp;
  nfsc_p e;
  size_t size;

  demand(filp, bogus filp);

  e = GETNFSCE(filp);
  size = nfsc_get_size(e);

  tmp = filp->f_pos;

  switch (whence) {
  case SEEK_SET:
    tmp = offset;
    break;
  case SEEK_CUR:
    tmp += offset;
    break;
  case SEEK_END:
    tmp = size + offset;
    break;
  default:
    errno = EINVAL;
    return -1;
    break;
  }
  if (tmp < 0) {errno = EINVAL; return -1;}

  filp->f_pos = tmp;
  return tmp;
}

int
nfs_chmod(struct file *filp, mode_t mode) {
  struct nfs_fh *fhandle;
  struct nfs_fattr temp_fattr;
  struct nfs_sattr temp_sattr;

  int status;

  DPRINTF(CLU_LEVEL,("nfs_chmod:\n"));
  demand(filp, bogus filp);

  fhandle = GETFHANDLE(filp);

  k2printf("nfs_chmod: nfs_proc_getattr\n");
  status = nfs_proc_getattr(fhandle,&temp_fattr);

  if (status == 0) {

    /* ALLPERMS is  07777 */
    temp_sattr.mode = ((temp_fattr.mode & ~ALLPERMS) |  (mode & ALLPERMS)); 

    DPRINTF(CLU_LEVEL,("nfs_chmod: nfs_proc_getattr returned %d, "
		       "new mode %08x, old %08x, setmode: %08x\n",
		       status,mode,temp_fattr.mode,temp_sattr.mode));
    /* do only permissions, also should use macro definitions */
    temp_sattr.uid  = -1; 
    temp_sattr.gid  = -1; 
    temp_sattr.size = -1; 
    temp_sattr.atime.seconds  = -1;
    temp_sattr.atime.useconds = -1;
    temp_sattr.mtime.seconds  = -1;
    temp_sattr.mtime.useconds = -1;
    
    status = nfs_proc_setattr(fhandle,
			      &temp_sattr,
			      &temp_fattr);

    DPRINTF(CLU_LEVEL,("nfs_chmod: nfs_proc_setattr returned %d\n",status));
    if (status == 0) {
      filp->f_mode = temp_sattr.mode;
      nfs_fill_stat(&temp_fattr, GETNFSCE(filp));
    }

    return(status);}
  else {
    fprintf(stderr,"warning: nfs_chmod, was not able to do getattr\n"
	   "on a NFS filp\n");
    errno = EACCES;
    return -1;
  }

}

int
nfs_truncate(struct file *filp, off_t length) {
  struct nfs_fh *fhandle;
  struct nfs_fattr temp_fattr;
  struct nfs_sattr temp_sattr;

  int status;
  /* make sure you interact with bc to drop unneeded page */
  DPRINTF(CLU_LEVEL,("nfs_truncate:\n"));
  demand(filp, bogus filp);

  fhandle = GETFHANDLE(filp);
  
  /* HBXX this is OVERKILL, should not flush all things. */
  nfsc_sync(GETNFSCE(filp));	/* just to be sure the size if proper */

  k2printf("nfs_truncate: nfs_proc_getattr\n");
  status = nfs_proc_getattr(fhandle,&temp_fattr);

  if (status == 0) {
    temp_sattr.mode = temp_fattr.mode;
    /* do only permissions, also should use macro definitions */
    temp_sattr.uid  = -1;
    temp_sattr.gid  = -1;
    temp_sattr.size = length;
    temp_sattr.atime.seconds  = -1;
    temp_sattr.atime.useconds = -1;
    temp_sattr.mtime.seconds  = -1;
    temp_sattr.mtime.useconds = -1;
    
    status = nfs_proc_setattr(fhandle,
			      &temp_sattr,
			      &temp_fattr);

    if (status == 0) {
      /* note that the f_pos is not changed */
      nfs_flush_filp(filp);
      nfs_fill_stat(&temp_fattr, GETNFSCE(filp));
      return(0);
    }
    errno = status;
    return(-1);
  } else {
    fprintf(stderr,"warning: nfs_truncate, was not able to do getattr\n"
	   "on a NFS filp\n");
    errno = status;
    return -1;
  }
}

int
nfs_chown(struct file *filp, uid_t owner, gid_t group) {
  struct nfs_fh *fhandle;
  struct nfs_fattr temp_fattr;
  struct nfs_sattr temp_sattr;

  int status;

  kprintf("nfs_chown: bogusly returning success for ease of use.\n");
  return 0; /* XXX */

  DPRINTF(CLU_LEVEL,("nfs_chown:\n"));
  demand(filp, bogus filp);

  fhandle = GETFHANDLE(filp);
#if 0
  k2printf("nfs_chown: nfs_proc_getattr\n");
  status = nfs_proc_getattr(fhandle,&temp_fattr);
#else
  status = 0;
#endif
  if (status == 0) {
    temp_sattr.mode = -1;
    temp_sattr.uid  = (owner == -1) ? -1 : owner;
    temp_sattr.gid  = (group == -1) ? -1 : group;
    temp_sattr.size = -1;
    temp_sattr.atime.seconds  = -1;
    temp_sattr.atime.useconds = -1;
    temp_sattr.mtime.seconds  = -1;
    temp_sattr.mtime.useconds = -1;
    
    status = nfs_proc_setattr(
			      fhandle,
			      &temp_sattr,
			      &temp_fattr);
    if (status == 0) {
      nfs_fill_stat(&temp_fattr, GETNFSCE(filp));
    }
    return(status);}
  else {
    fprintf(stderr,"nfs_chown, was not able to do getattr\n"
	     "on a NFS filp\n");
      errno = EACCES;
      return -1;
  }
  
}

/* The following X functions affect are the only ones that affect the name
 cache (and the directory cache):
 nfs_open (in file nfs_lookup_cache.c),
 nfs_lookup (in file nfs_lookup_cache.c),
 nfs_link,
 nfs_symlink,
 nfs_mkdir,
 nfs_rmdir,
 nfs_rename 
 */
int 
nfs_unlink(struct file *dirp, const char *name) {
  nfsc_p d,f;
  int dev;
  struct nfs_fh *fhandle;
  int status,lookup_status;

  DPRINTF(CLU_LEVEL,("** nfs_unlink %s\n",name));
  demand(dirp, bogus filp);
  d = GETNFSCE(dirp);
  fhandle = GETFHANDLE(dirp);
  dev = FHGETDEV(fhandle);

  lookup_status = nfs_cache_lookup(dev,GETNFSCEINO(d),name,&f);
  switch(lookup_status) {
  case -1: 
    /* negative cache hit, treat it as a miss to be sure is properly removed */
  case 0:
    /* cache miss */
    {
      struct nfs_fh fhandle2;
      struct nfs_fattr temp_fattr;
      
      k2printf("nfs_unlink: nfs_proc_lookup %s\n",name);
      status = nfs_proc_lookup(fhandle,
			       name,
			       &fhandle2,
			       &temp_fattr);

      if (status != 0) {
	errno = status;
	return -1;
      } 

      f = nfsc_get(dev,temp_fattr.fileid);
      nfs_fill_stat(&temp_fattr, f);
      
    }
  /* fall-through now that we have setup f */
  case 1:
    /* cache hit */
    if (nfsc_get_refcnt(f) == 1) {
      /* last copy */
      if (S_ISDIR(nfsc_get_mode(f))) {
        status = nfs_proc_rmdir(fhandle, name);
	//fprintf(stderr,"rmdir  status %d\n",status);
      } else {
	status = nfs_proc_remove(fhandle,name);
	//fprintf(stderr,"remove status %d\n",status);
      }
      if (status == 0) {
	nfs_cache_remove(dirp,NULL);
	nfs_flush_filp(dirp);
      }

      nfsc_put(f);
      if (status != 0) {
	errno = status; return -1;
      } 
      return 0;


    } else {
      /* has other references */
      static int i = 0;
      char difname[NAME_MAX];
      //fprintf(stderr,"removing a file that is still referenced: %s\n",name);
      demand (!(S_ISDIR(f->sb.st_mode)), removing last ref directory);
      if (i == 0) atexit(nfs_process_unlinkatend);
      sprintf(difname,".nfs%d%d.%s",time(0),i++,name);
      status = nfs_proc_rename(fhandle,name,fhandle,difname);
      if (status != 0) fprintf(stderr,"could not rename %s\n",name);
      nfs_cache_remove(dirp,name);
      nfs_flush_filp(dirp);
      nfs_unlinkatend(difname, fhandle);
      nfsc_put(f);
      
      /* since it will be unlinked we can avoid flushing on close */
      nfsc_or_flags(f,NFSCE_WILLBEGONE); 

      return 0;
    }
  default:
    PR;
    assert(0);
    return 0;
  }
}

int 
nfs_link(struct file *filp, struct file *dir_filp, 
	   const char *name) {
  struct nfs_fh *fhandle, *dhandle;
  int status;

  DPRINTF(CLU_LEVEL,("nfs_link:\n"));
  demand(filp, bogus filp);
  demand(dir_filp, bogus dir_filp);

  fhandle = GETFHANDLE(filp);
  dhandle = GETFHANDLE(dir_filp);

  status = nfs_proc_link(fhandle,dhandle,name);
  /* HBXX - the nlink field in the stat structure will be invalid */
  if (status == 0) {
    nfs_flush_filp(dir_filp);
    nfs_cache_remove(dir_filp,name);
    return 0;
  } else {
    errno = status; return -1;
  }
}

int 
nfs_symlink(struct file *dir_filp, 
	   const char *name,const char *to) {
  struct nfs_fh *dhandle;

  struct nfs_sattr temp_sattr;

  int status;

  DPRINTF(CLU_LEVEL,("nfs_symlink:\n"));
  demand(dir_filp, bogus dir_filp);

  dhandle = GETFHANDLE(dir_filp);

  temp_sattr.mode = 0100666; 
  temp_sattr.uid  = __current->uid;
  temp_sattr.gid  = __current->egid;
  temp_sattr.size = strlen(to);
  temp_sattr.atime.seconds  = -1;
  temp_sattr.atime.useconds = -1;
  temp_sattr.mtime.seconds  = -1;
  temp_sattr.mtime.useconds = -1;
  status = nfs_proc_symlink(dhandle,
			    name,
			    to,
			    &temp_sattr);
  
  if (status == 0) {
    nfs_flush_filp(dir_filp);
    nfs_cache_remove(dir_filp,name);
    return 0;
  } else {
      errno = status; return -1;
  }
}

int 
nfs_mkdir(struct file *dir_filp, const char *name, mode_t mode) {
  struct nfs_fh *dhandle, temp_fhandle;
  struct nfs_fattr temp_fattr;
  struct nfs_sattr temp_sattr;
  int status;

  DPRINTF(CLU_LEVEL,("nfs_mkdir:\n"));
  demand(dir_filp, bogus dir_filp);
  demand(name, empty name given to nfs_mkdir);

  dhandle = GETFHANDLE(dir_filp);

  k2printf("nfs_mkdir: nfs_proc_getattr\n");
  status = nfs_proc_getattr(dhandle,&temp_fattr);

  if (status != 0) {
    fprintf(stderr,"nfs_mkdir, was not able to do getattr\n"
	     "on a NFS filp\n");
      errno = status;
      return -1;
  }
  
  temp_sattr.mode = ((temp_fattr.mode & 0xfffffe00) | (mode & 0x000001ff));
  temp_sattr.uid  = __current->uid;
  temp_sattr.gid  = (temp_fattr.gid) ? temp_fattr.gid : __current->egid;
  temp_sattr.size = 1024;
  temp_sattr.atime.seconds  = -1;
  temp_sattr.atime.useconds = -1;
  temp_sattr.mtime.seconds  = -1;
  temp_sattr.mtime.useconds = -1;
  status = nfs_proc_mkdir(dhandle,
			  name,
			  &temp_sattr,
			  &temp_fhandle,
			  &temp_fattr);
  
  
  nfs_flush_filp(dir_filp);
  nfs_cache_remove(dir_filp,name);
  if (status == 0) {
    nfs_cache_add_devino(FHGETDEV(dhandle),temp_fattr.fileid,"..", 
			 GETNFSCE(dir_filp));
    return 0;
  } else {
    errno = status; return -1;
  }
}

int 
nfs_rmdir(struct file *dir_filp, const char *name) {
  struct nfs_fh *dhandle;
  int status;

  fprintf(stderr,"nfs_rmdir %p ino: %d name: %s\n",dir_filp, 
	  dir_filp->f_ino, name);
  DPRINTF(CLU_LEVEL,("nfs_rmdir:\n"));
  demand(dir_filp, bogus dir_filp);
  demand(name, empty name given to nfs_rmdir);

PR;
  dhandle = GETFHANDLE(dir_filp);

PR;
  status = nfs_proc_rmdir(dhandle, name);
PR;
   fprintf(stderr,"status %d\n",status);
  
PR;
  nfs_cache_remove(dir_filp,name);
PR;
  if (status == 0) {
    nfs_flush_filp(dir_filp);
    return 0;
  } else {
      errno = status; return -1;
  }
}

int 
nfs_rename(struct file *filpfrom, const char *namefrom, 
	   struct file *filpto,   const char *nameto) {
    /* it will be guaranteed that both filp are the same type
       I guess I will have to make sure that they are in the same 
       server ??
       */
    struct nfs_fh *fhandlefrom, *fhandleto;
    int status;

    DPRINTF(CLU_LEVEL,("nfs_rename:\n"));

    if (namefrom == NULL || nameto == NULL) {
	errno = EFAULT;
	return -1;
    } else if (*namefrom == (char)NULL || *nameto == (char)NULL) {
	errno = ENOENT;
	return -1;
    }
    
    demand(filpfrom, bogus filp);
    demand(filpto, bogus filp);
    demand(filpfrom->op_type == filpto->op_type,
	   filpfrom and filpto are of different types!!);
    
    fhandlefrom = GETFHANDLE(filpfrom);
    fhandleto = GETFHANDLE(filpto);

    status = nfs_proc_rename(fhandlefrom,namefrom,
			     fhandleto, nameto);
			     
    if (status != 0) {
	errno = status;
	return -1;
    } else {
      nfs_cache_remove(filpfrom,namefrom);
      nfs_cache_remove(filpto,nameto); /* should be adding this entry */
      nfs_flush_filp(filpfrom);
      if (filpfrom != filpto) nfs_flush_filp(filpto);
      return 0;
    }
}




int 
nfs_select(struct file *filp,int rw) {
  demand(filp, bogus filp);
  return 1;
}


inline void 
nfs_fill_stat(struct nfs_fattr *temp_fattr, nfsc_p e) {
  struct stat buf0;
  struct stat *buf = &buf0;

  buf->st_dev     = temp_fattr->fsid;
  buf->st_ino     = temp_fattr->fileid;
  buf->st_mode    = make_st_mode(temp_fattr->mode,temp_fattr->type);
  buf->st_nlink   = temp_fattr->nlink;
  buf->st_uid     = temp_fattr->uid;
  buf->st_gid     = temp_fattr->gid;
  buf->st_rdev    = temp_fattr->rdev;
  buf->st_size    = temp_fattr->size;
  buf->st_atime   = (time_t) temp_fattr->atime.seconds;
  buf->st_atimensec   = (long) temp_fattr->atime.useconds * 1000;
  buf->st_mtime   = (time_t) temp_fattr->mtime.seconds;
  buf->st_mtimensec   = (long) temp_fattr->mtime.useconds * 1000;
  buf->st_ctime   = (time_t) temp_fattr->ctime.seconds;
  buf->st_ctimensec   = (long) temp_fattr->ctime.useconds * 1000;
  buf->st_blksize = (u_int32_t) temp_fattr->blocksize;
  buf->st_blocks  = (int64_t) temp_fattr->blocks;

  nfsc_set_sb(e,*buf);
  if (nfsc_get_writeend(e) > nfsc_get_size(e))
    nfsc_set_size(e,nfsc_get_writeend(e));
}

inline int
nfs_stat_fh(struct nfs_fh *fhandle, nfsc_p e) {
  struct nfs_fattr temp_fattr;
  int status;

  k2printf("nfs_stat_fh : nfs_proc_getattr\n");
  status = nfs_proc_getattr(fhandle,&temp_fattr);
    
  if (status != 0) {
    k1printf("nfs_stat_fh, was not able to do getattr\n"
	     "on a NFS filp status: %d\n",status);
    errno = status;
    return -1;
  }
  nfs_fill_stat(&temp_fattr, e);
  return(0);

}

int 
nfs_stat(struct file *filp, struct stat *buf) {
    nfsc_p e;
    int dev,ino;
    int status = 0;

    DPRINTF(CLU_LEVEL,("nfs_stat: ino %d\n",filp->f_ino));
    demand(filp, bogus filp);

    e = GETNFSCE(filp);
    
    dev = GETNFSCEDEV(e);
    ino = GETNFSCEINO(e);

    status = nfsc_checkchange(e, NFSSTATMIN);
    switch(status) {
    case 0:
      nfsc_copyout_sb(e,*buf);
       break;
    case 1:
      nfs_flush_filp(filp);
      /* if a directory has changed, we remove all names cached for it */
      if (S_ISDIR(e->sb.st_mode)) {nfs_cache_remove(filp,NULL);}
      *buf = e->sb;
      break;
    case -1:
      /* errno is set by nfsc_checkchange */
      DPRINTF(CLU_LEVEL,("nfs_stat: nfsc_checkchange failed errno: %d\n",errno));
      return status;
    }
    buf->st_dev = GETNFSCEDEV(e);
    return 0;
}


int
nfs_utimes(struct file *filp, const struct timeval *times) {
  struct nfs_fh *fhandle;
  struct nfs_fattr temp_fattr;
  struct nfs_sattr temp_sattr;

  int status;

  DPRINTF(CLU_LEVEL,("nfs_utimes:\n"));
  demand(filp, bogus filp);

  fhandle = GETFHANDLE(filp);
#if 0
  fprintf(stderr,"nfs_utimes: nfs_proc_getattr\n");
  status = nfs_proc_getattr(fhandle,&temp_fattr);
#else
  status = 0;
#endif
  if (status == 0) {
    temp_sattr.mode = -1;
    temp_sattr.uid  = -1;
    temp_sattr.gid  = -1;
    temp_sattr.size = -1;

    if (times) {
      //fprintf(stderr,"time passed in\n");
      temp_sattr.atime.seconds  = times[0].tv_sec;
      temp_sattr.atime.useconds = times[0].tv_usec;
      temp_sattr.mtime.seconds  = times[1].tv_sec;
      temp_sattr.mtime.useconds = times[1].tv_usec;
    } else {
      struct timeval times2;
      gettimeofday(&times2,(struct timezone *)0);
      //fprintf(stderr,"times2.tv_sec = %ld\n",times2.tv_sec);
      temp_sattr.atime.seconds  = times2.tv_sec;
      temp_sattr.atime.useconds = times2.tv_usec;
      temp_sattr.mtime.seconds  = times2.tv_sec;
      temp_sattr.mtime.useconds = times2.tv_usec;
    }
    /* the owner will be the only one allowed to change the file */
    status = nfs_proc_setattr(fhandle,
			      &temp_sattr,
			      &temp_fattr);

    //fprintf(stderr,"status: %d errno %d\n",status,errno);
    if (status == 0) {
      nfsc_p e = GETNFSCE(filp);
      nfs_fill_stat(&temp_fattr,e);
      nfs_flush_filp(filp);
    }

    
    return(status);
  } else {
    fprintf(stderr,"warning: nfs_utimes, was not able to do getattr\n"
	   "on a NFS filp\n");
    errno = EACCES;
    return -1;
  }

}

int 
nfs_readlink(struct file *filp, char *buf, int bufsize) {
    struct nfs_fh *fhandle;

    int status;

    DPRINTF(CLU_LEVEL,("nfs_readlink:\n"));
    demand(filp, bogus filp);
    if (buf == (char *) 0) {errno = EFAULT; return -1;}
    
    fhandle = GETFHANDLE(filp);
    
    status = nfs_proc_readlink(fhandle,buf);
    
    if (status != 0) {
	errno = status;
	return -1;
    } else {
	return strlen(buf);
    }
}
 




static struct uae {
  char name[NAME_MAX];
  struct nfs_fh fh;
  struct uae *next;
} *uae_base = NULL;

static void nfs_process_unlinkatend(void) {
  struct uae *p;
  //fprintf(stderr,"NFS_PROCESS_UNLINKATEND \n");
  for (p = uae_base; p != NULL ; p = p->next) {
    //fprintf(stderr,"REMOVING %s\n",(char *)&p->name[0]);
    nfs_proc_remove(&p->fh,(char *)&p->name[0]);
  }
}
static void nfs_unlinkatend(char *name, struct nfs_fh *fhandle) {
  struct uae *p;

  //fprintf(stderr,"NFS_UNLINKATEND name: %s\n",name);
  p = (struct uae *)__malloc(sizeof (struct uae));
  if (p == NULL) {
    fprintf(stderr,
	      "could not allocate space for removing nfs file while it is referenced\n");
    return;
  }
  strcpy(&p->name[0], name);
  memcpy(&p->fh, fhandle, sizeof(struct nfs_fh));
  
  p->next = uae_base;
  uae_base = p;
  
}
