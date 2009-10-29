
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

#include "nfs_rpc.h"
#include "nfs_rpc_procs.h"
#include "nfs_proc.h"

#include "errno.h"
#include <exos/debug.h>

#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <sys/times.h>

#include <fd/proc.h>
#include <fd/path.h>
#include "nfs_xdr.h"

#include <dirent.h>


#include <exos/vm-layout.h>		/* for PAGESIZ */
#include <exos/vm.h>			/* for PG_SHARED */
#include <xok/sys_ucall.h>

#include <xok/pxn.h>
#include <xok/bc.h>
#include <xok/sysinfo.h>

#include <exos/ubc.h>
#include <exos/uwk.h>

#include <exos/critical.h>	

#define k0printf if (0) kprintf
#define k1printf if (0) printf
static const int debugging = 0;
#define printf if (debugging) printf

#if 0
#define dprintf(format,args...) fprintf(stderr,format, ## args)
#else
#define dprintf(format,args...) 
#endif

#if 0
#define d2printf(format,args...) fprintf(stderr,format, ## args)
#else
#define d2printf(format,args...) 
#endif

#if 0
#define pprintf(format,args...) fprintf(stderr,format, ## args)
#else
#define pprintf(format,args...)
#endif

#undef PR
#if 0
#define PR fprintf(stderr,"@ %s:%d\n",__FILE__,__LINE__)
#else
#define PR
#endif


int
nfs_getdirentries_refresh(struct file *filp);

static int print_entry(int total,struct dirent *d) {
  printf("total: %4d ino: %6u rlen: %3u type: %2x nlen: %2u name: %s\n",
	 total,d->d_fileno, d->d_reclen, d->d_type, d->d_namlen, d->d_name);
  return d->d_reclen;
}
#if 0
static void print_entries(char *buf,int length) {
  int internallimit,l;
  int total = 0;
  internallimit = 1000;
  fprintf(stderr,"print_entries: %p, length: %d\n", buf, length);
  while(length > 0 && internallimit-- > 0) {
      print_entry(total,(struct dirent *)buf);
      l = ((struct dirent *)buf)->d_reclen;
      total += l;
      buf += l;
      length -= l;
  }
}
#else
#define print_entries(x,y)
#endif

/* 
 * XXX - I am not sure if the following code is still kept up and coherent
 * with nfs_getdirentries_cached
 */
int
nfs_getdirentries(struct file *filp, char *buf0, int nbytes) {
    struct nfs_fh *fhandle;
    int *p, *p0;
    int ruid = 0;
    int size;
    int total = 0;
    int reclen,len;
    struct generic_server *server;
    int status,cookie;
    int *buf = (int *) buf0;
    struct dirent *dirent;
    nfsc_p e;

    if (S_ISREG(filp->f_mode)) {errno = EINVAL; return -1;}
    if (filp->f_flags & O_WRONLY) {errno = EBADF; return -1;} 

    if (filp->f_pos == 0) nfs_getdirentries_refresh(filp);
    
    DPRINTF(CLU_LEVEL,
	    ("nfs_getdirentries: \n   filp: %08x data: %p offset: %d nbyte: %d\n",
	     (int)filp, buf0, (int)filp->f_pos, nbytes));
    demand(filp, bogus filp);
    if (buf == (int *) 0) {errno = EFAULT; return -1;}
    
    fhandle = GETFHANDLE(filp);
    server  = fhandle->server;
    size = MIN(NFS_MAXRDATA,nbytes);
    cookie = filp->f_pos;
    e = GETNFSCE(filp);
    if (!cookie) e->n_dirsize = 0; /* initialize */

    if (cookie && cookie == e->n_dirsize) return 0;

    if (!(p0 = overhead_rpc_alloc(server->rsize)))
      return -EIO;
retry:
    p = nfs_rpc_header(p0, NFSPROC_READDIR, ruid);
    p = xdr_encode_fhandle(p, fhandle);
    *p++ = htonl(cookie);
    *p++ = htonl(size);
    if ((status = generic_rpc_call(server, p0, p)) < 0) {
      overhead_rpc_free(p0);
      return -1;
    }
    if (!(p = generic_rpc_verify(p0))) {errno = EIO; return -1;}
    else if ((status = ntohl(*p++)) == NFS_OK) {
      reclen = 0;
      for ( total = 0; *p++ && total < nbytes;) {
	/* unsigned long   d_fileno; */
	/* unsigned short  d_reclen; */
	/* unsigned short  d_namlen; */
	/* char            d_name[MAXNAMELEN + 1]; */
	dirent = (struct dirent *) buf;
	dirent->d_fileno = ntohl(*p++);		    /* copy d_fileno */
	dirent->d_type = 0;
	dirent->d_namlen = len = ntohl(*p++);	    /* copy d_namlen */
	memcpy((char *)&dirent->d_name, (char *) p, len); /* copy d_name */
	dirent->d_name[len] = (char)0;	    /* null terminate d_name */
	p += (len + 3) >> 2;				    /* move p */
	cookie = ntohl(*p++);		    /* save cookie */
	len = (len + 1 + 3) >> 2;
	total += (len << 2) + 8;
	reclen = dirent->d_reclen = (len << 2) + 8;
	buf += (len + 2);
      }
      if (*p) {e->n_dirsize = cookie;} /* EOF has been reached */

      filp->f_pos = cookie;
  } else {
    if (!ruid && __current->fsuid == 0 && __current->uid != 0) {
      ruid = 1;
      k1printf("GOTO RETRY\n");
      goto retry;
    }
    DPRINTF(CLUHELP_LEVEL,("NFS reply readdir failed = %d\n", status));
  }
  overhead_rpc_free(p0);
  if (status == NFS_OK) {return total;} else {
    errno = nfs_stat_to_errno(status);
    return -1;
  }
}

int
nfs_getdirentries_cached(struct file *filp, char *buf, int nbytes) {
  int ino,dev,status;
  u_quad_t block;
  struct bc_entry *bc_entry;
  /* because it can be used at other function invocations */
  char *bcaddr;
  off_t EnterOffset;
  nfsc_p e;
  int pageno;
  int leftOver = -1, blockOffset, amountToCopy;

#ifdef __NFS3_READDIR_PROB__
  /* XXX - hack: until nfs3 bug is fixed, we don't let nfs3 clients cache
   * readdir */
  int gotnew = 0;
#endif

  demand(filp, bogus filp);
  DPRINTF(CLU_LEVEL,
	  ("nfs_getdirentries_cached: filp: %p, data: %p offset: %d nbyte: %d\n",
	   filp, buf, (int)filp->f_pos, nbytes));


  if (S_ISREG(filp->f_mode)) {
    errno = EINVAL; 
    return -1;}
  if (filp->f_flags & O_WRONLY) {errno = EBADF; return -1;} 
  if (nbytes < NFS_DIRBLKSZ) {errno = EINVAL; return -1;}

  e = GETNFSCE(filp);

  pprintf("nfs_getdirentries_cached: filp: %p, data: %p nbyte: %d offset: %d size: %d\n",
	   filp, buf , nbytes,(int)filp->f_pos,e->n_dirsize);

  ino = GETNFSCEINO(e);
  dev = GETNFSCEDEV(e);
  
  EnterOffset = filp->f_pos;
  nbytes &= ~(NFS_DIRBLKSZ - 1); /* blksz align nbytes */

  while(nbytes > 0) {
    pageno = filp->f_pos / NFSPGSZ;
    blockOffset = filp->f_pos % NFSPGSZ;
    amountToCopy = min (nbytes, NFSPGSZ - blockOffset);

    dprintf("offset: %d pageno: %d, blkoffset %d, tocpy: %d, leftover %d\n",
	    (int) filp->f_pos, pageno, blockOffset, amountToCopy, leftOver);

    block = NFSBCBLOCK(ino,pageno);
  retry:
    /* if entries do not exist or we dont know how large the directory is */
    if ((bc_entry = __bc_lookup64 ((u32)dev, block)) == NULL || e->n_dirsize == 0) {
      dprintf("__bc_lookup block missing: pageno %d\n",pageno);
    readnew:
#ifdef __NFS3_READDIR_PROB__
      gotnew = 1;
#endif
      dprintf("READNEW\n");
      nfs_getdirentries_refresh(filp);
      /* possible that the directory shrank */
      if ((e->n_dirsize - filp->f_pos) <= 0) goto done;
      
      goto retry;
    }
    /* if BC_EMPTY or BC_COMING_IN, for now, be inefficient, reread */
    if (bc_entry->buf_state != BC_VALID) {
      fprintf(stderr,"BC_ENTRY->buf_state %d, pageno: %d\n",bc_entry->buf_state,pageno);
      nfs_flush_nfsce(e);
      goto readnew;
    }
  
    status = nfsc_checkchange(e,NFSDIRMIN);
#ifdef __NFS3_READDIR_PROB__
    if (gotnew == 0) status = 1;
#endif
    switch(status) {
    case 1:
      /* changed */
      nfs_flush_nfsce(e);
      goto readnew;
    case 0:

      leftOver = e->n_dirsize - filp->f_pos;

      /* it is possible that the directory has shrank */
      if (leftOver <= 0) goto done;

      amountToCopy = min (amountToCopy, leftOver);

      assert(bc_entry);
      bcaddr = new_getbcmapping(bc_entry);	
      assert(bcaddr);
      assert(((uint)bcaddr & 0xfff) == 0);

      assert(amountToCopy >= 0);
      assert(amountToCopy > 0);
      memcpy(buf,bcaddr + blockOffset, amountToCopy);
      DEALLOCATE_MEMORY_BLOCK(bcaddr,NFSPGSZ);

      buf += amountToCopy;
      nbytes -= amountToCopy;
      leftOver -= amountToCopy;
      filp->f_pos += amountToCopy;

      if (leftOver == 0) goto done;
    
    break;
    case -1:
      /* dont know how to handle stale stuff in getdirents */
      assert(0);
      PR;
      return -1;
    }
  }
  dprintf("RETURNING offset %d EnterOffset %d\n",(int)filp->f_pos,(int)EnterOffset);
  PR;
done:
  return (filp->f_pos - EnterOffset);

}


#define QUADH(q) (((long *)(&(q)))[_QUAD_HIGHWORD])
#define QUADL(q) (((long *)(&(q)))[_QUAD_LOWWORD])

static struct bc_entry *__bc_entry;
static char *__vaddr;

char *
nfs_getdirentries_getpage(nfsc_p e, unsigned short pageno) {
  int dev, ino, status;
  u_quad_t block;
  
  pprintf("nfs_getdirentries_getpage e: %p, pageno: %d\n",e,pageno);
  assert(e);

  dev = GETNFSCEDEV(e);
  ino = GETNFSCEINO(e);
  
  block = NFSBCBLOCK(ino,pageno);

  EnterCritical();
  __bc_entry = __bc_lookup64 ((u32)dev, block);
  
  if (__bc_entry == NULL) {
    ExitCritical();
    /* HBXX should really inserted here with BC_COMING_IN */
    __vaddr = ALLOCATE_BC_PAGE();
    assert(__vaddr);
  } else {

    if (__bc_entry->buf_state != BC_VALID) {
      ExitCritical();
      assert(0); /* block until page is in */
      wk_waitfor_value((int *)(&__bc_entry->buf_state),BC_VALID,NFS_CAP);
    }
    if ((status = sys_bc_set_state64(dev,QUAD2INT_HIGH(block),QUAD2INT_LOW(block),BC_COMING_IN)) < 0) {
      ExitCritical();
      kprintf("sys_bc_set_state64((%d,%d),BC_COMING_IN) failed: %d\n",
	      dev, ino, status);
      assert(0);
    }
    ExitCritical();
    /* this will make sure that the entry is locked in */
    __vaddr = new_getbcmapping(__bc_entry);	
    /* race condition between __bc_lookup and mapping the page */
    assert(__vaddr);	 
  }
  return __vaddr;
}

void
nfs_getdirentries_putpage(nfsc_p e, short pageno, int unmaponly) {
  int dev, ino, status;
  u_quad_t block;

  pprintf("nfs_getdirentries_putpage e: %p, pageno: %d unmaponly: %d\n",
	  e,pageno,unmaponly);
  assert(e);

  if (unmaponly) goto unmap;

  dev = GETNFSCEDEV(e);
  ino = GETNFSCEINO(e);
  
  block = NFSBCBLOCK(ino,pageno);

  if (__bc_entry == NULL) {
    /* we allocated the page */
    __bc_entry = nfs_new_bc_insert(__vaddr,dev,block); 
    assert(__bc_entry);
    assert(__bc_entry->buf_state == BC_VALID);
  } else {
    /* otherwise it was coming in */
    if ((status = sys_bc_set_state64(dev,QUAD2INT_HIGH(block),QUAD2INT_LOW(block),BC_VALID)) < 0) {
      kprintf("sys_bc_set_state64((%d,%d),BC_VALID) failed: %d\n",
	      dev, ino, status);
      assert(0);
    }
  }
unmap:
  DEALLOCATE_MEMORY_BLOCK(__vaddr,NFSPGSZ);
}

int
nfs_getdirentries_refresh(struct file *filp) {
    struct nfs_fh *fhandle;
    struct generic_server *server;
    struct dirent *dirent,*prev_dirent;
    nfsc_p e;
    int *p, *p0;
    int ruid = 0,size,total = 0, len,status, length,pageoffset,pageno;
    int cookie;
    int lastcookiesent;
    char *buf, *buf0;
    
    demand(filp, bogus filp);
    
    fhandle = GETFHANDLE(filp);
    server  = fhandle->server;
    size = NFS_MAXRDATA;
    e = GETNFSCE(filp);


    len = 0;
    pageoffset = 0;
    total = 0;
    cookie = 0;
    lastcookiesent = -1;
    dirent = prev_dirent = (struct dirent *)0;
    pageno = 0;
    buf0 = buf = nfs_getdirentries_getpage(e,pageno);

    for(;;) {
      /* Loop until eof entry. */
      if (!(p0 = overhead_rpc_alloc(server->rsize))) {
	nfs_getdirentries_putpage(e,pageno,1);
	errno = EIO; return -1;
      }

      p = nfs_rpc_header(p0, NFSPROC_READDIR, ruid);
      p = xdr_encode_fhandle(p, fhandle);
      *p++ = htonl(cookie);
      *p++ = htonl(size);

      printf("DOING RPC CALL WITH COOKIE %d\n",cookie);
      if ((length = generic_rpc_call(server, p0, p)) < 0 ||
	  !(p = generic_rpc_verify(p0))) {
	nfs_getdirentries_putpage(e,pageno,1);
	overhead_rpc_free(p0);
	return -1;
      }
      if (debugging) print_rpc2(p,length);
      /* assertions: pageoffset is never > NFS_DIRBLKSZ */
      status = ntohl(*p++);
      assert(status == NFS_OK);
      /* p at the top of loop points at: are there more entries? */
      printf("BEGINNING NEXTENTRY VALID %d EOF: %d PEEK: %d\n",htonl(p[0]),htonl(p[1]),htonl(p[2]));

      if (cookie == lastcookiesent) {
	/* HBXX - there is a bug with openbsd nfs server on sparse directories
	 (those on which many files have been created, then deleted creating) 
	 basically if the size you ask is too small, it will not set the EOF
	 on the NFS reply, thereby leading the client into an infinite loop.
	 this is a problem with our software, because we have no reassambly. */
	kprintf("BUGGY SERVER SOFTWARE, using counter measures\n");
	kprintf("buggy directory inode: %d\n",filp->f_ino);
	/* simulating end */
	p[0] = htonl(0);
	p[1] = htonl(1);
      }
      lastcookiesent = cookie;

      if (cookie != 0 && *p) {
	/* invariant: on after the first rpc call, we need to skip the first
	 * entry since, the cookie we sent has been already copied */
	int cookie2;
	int *q = p;
	/* we are not doing first read, and entry is valid */
	/* need to skip this entry since it will be the same as the last one */
	q++;			/* skip valid */
	q++;			/* skip fileid */
	len = ntohl(*q++);	/* name length */
	q += (len + 3) >> 2;	/* skip name */
	cookie2 = ntohl(*q);
	q++;			/* skip cookie */
	if (cookie2 != cookie) {
	  printf("INVARIANT FALIURE cookie: %d, cookie2: %d\n",cookie,cookie2);
	} else {
	  printf("SKIPPING cookie: %d\n",cookie2);
	  p = q;
	}
      } else {
	printf("FIRST READ OR ENTRY NOT VALID\n");
      }
      for ( ; *p++ ;) {
	printf("NEXTENTRY IS VALID\n");
	/* unsigned long   d_fileno; */
	/* unsigned short  d_reclen; */
	/* unsigned short  d_namlen; */
	/* char            d_name[MAXNAMELEN + 1]; */

	/* need to account for null termination */
	len = ((ntohl(p[1])+1 + 3) & ~3) + 8; /* dirent entry length */
	
	if (len + pageoffset > NFS_DIRBLKSZ) {
	  d2printf("NEW BLOCK len: %2d, pageoffset: %3d total: %d pageno: %d\n",
		  len, pageoffset,total,pageno);
	  prev_dirent->d_reclen += (NFS_DIRBLKSZ - pageoffset);
	  buf += (NFS_DIRBLKSZ - pageoffset);
	  total += (NFS_DIRBLKSZ - pageoffset);
	  pageoffset = 0;
	}
	if ((buf - buf0) == NFSPGSZ) {
	  d2printf("STARTING NEW PAGE old pageno %d, total: %d, len:%d\n",
		  pageno,total,len);
	  print_entries(buf0,NFSPGSZ);
	  nfs_getdirentries_putpage(e,pageno,0);	
	  pageno++;
	  buf0 = buf = nfs_getdirentries_getpage(e,pageno);
	}

	dirent = (struct dirent *) buf;
	dirent->d_fileno = ntohl(*p++);		    /* copy d_fileno */
	dirent->d_type = 0;
	dirent->d_namlen = len = ntohl(*p++);	    /* copy d_namlen */
	memcpy((char *)&dirent->d_name, (char *) p, len); /* copy d_name */
	dirent->d_name[len] = (char)0;	    /* null terminate d_name */
	p += (len + 3) >> 2;				    /* move p */
	cookie = ntohl(*p++);		    /* save cookie */
	len = (len + 1 + 3) >> 2; /* include null termination */
	total += (len << 2) + 8;

	pageoffset += (len << 2) + 8;
	if (pageoffset == NFS_DIRBLKSZ) {
	  d2printf("NEW BLOCK NO ALIGN len: %2d, pageoffset: %3d total: %d pageno: %d\n",
		  len, pageoffset,total,pageno);

	  pageoffset = 0; /* reset if dirblksz aligned */

	}
	dirent->d_reclen = (len << 2) + 8;
	buf += dirent->d_reclen;
	prev_dirent = dirent;

	print_entry(total,dirent);
	printf("NEXTENTRY VALID %d EOF: %d PEEK: %d\n",htonl(p[0]),htonl(p[1]),htonl(p[2]));
      }
      if (*p) {			/* EOF */
	printf("EOF FOUND\n");
	if (pageoffset) {	/* align last dirent if necessary */
	  total += (NFS_DIRBLKSZ - pageoffset);
	  prev_dirent->d_reclen += (NFS_DIRBLKSZ - pageoffset);
	}
	print_entries(buf0, total - pageno*NFSPGSZ);

	e->n_dirsize = total;

	nfs_getdirentries_putpage(e,pageno,0);
	break;
      }
    }
    overhead_rpc_free(p0);
    if (status == NFS_OK) {return total;} else {
      errno = nfs_stat_to_errno(status);
      return -1;
    }
}
