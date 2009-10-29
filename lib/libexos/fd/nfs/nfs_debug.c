
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
#include "nfs_struct.h"
#include "nfs_proc.h"

#include "errno.h"
#include <exos/debug.h>

#include <ctype.h>

#undef PR
#if 0
#define PR fprintf(stderr,"@ %s:%d\n",__FILE__,__LINE__)
#else
#define PR
#endif

#define k1printf if (0) kprintf
#define fprintf  if (0) fprintf

/* used only for pr_filp */
static void 
pr2_nfsc(nfsc_p p);

void
pr_filp(struct file *filp,char *label) {
    int i;
    struct nfs_fh *fhandle;
    unsigned short *data;
    char *filp_type;
    if (filp == (struct file *)0 || label == NULL) {
      fprintf(stderr,"pr_filp %p: bogus filp: %p\n",label,filp);
      return;
    }
    demand(filp, bogus filp);
    demand(label, emtpy label);

    filp_type =  filp_op_type2name(filp->op_type);

    fprintf(stderr,"(%d) %10s %20s filp: %p\n",filp->op_type,filp_type,label,filp);
    fprintf(stderr,"f_flags: %05x   f_mode: %08x   f_count: %5d\n",
	    filp->f_flags,filp->f_mode,filp_refcount_get(filp));
    fprintf(stderr," f_pos: %8qd   f_owner: %5d      lock: %5d\n",
	    filp->f_pos,filp->f_owner,filp->lock.lock);
    fprintf(stderr," f_dev: %-8x   f_ino: %8d\n",
	    filp->f_dev,filp->f_ino);


    if (filp->op_type == UDP_SOCKET_TYPE) {
      extern void pr_udpfilp(void *);
      pr_udpfilp((void *)filp->data);
    } else if (filp->op_type != NFS_TYPE) {
      data = (unsigned short *)filp->data;
      for (i = 0; i < 16 ;i++) {
	if (i == 8) fprintf(stderr,"\n");
	fprintf(stderr,"%04x ",data[i]);
      } 
      fprintf(stderr,"\n");
    } else {
      nfsc_p p;

      p = GETNFSCE(filp);
//      fprintf(stderr,"nfsce: %d ",NFSCENUM(p));
      if(p) {
	fhandle = GETFHANDLE(filp);
//	fprintf(stderr,"serverp: %p ",fhandle->server);
//	fprintf(stderr,"server dev: %d \nfh: ",FHGETDEV(fhandle));
	if (filp->f_dev != FHGETDEV(fhandle)) {
	  fprintf(stderr,"WARNING MISMATCHING SERVER AND FILP DEV\n");
	}
	pr2_nfsc(p);
      } else {
	extern void __stacktrace_light(void);
	fprintf(stderr,"NULL NFSCE!!\n");
	__stacktrace_light();
      }
      fprintf(stderr,"\n");
    }
    return;
}


void 
pr_nfsc(nfsc_p p) {
  if (p->inuse) printf("*");

  printf("*nfsce: %p refcnt: %d dev: %d ino: %d\n",
	 p,p->refcnt, p->dev,p->sb.st_ino);

//  printf("  fence1: %08x == %08x, fence2: %08x == %08x\n",
//	 p->fence1, NFSCACHE_FENCE1,
//	 p->fence2, NFSCACHE_FENCE2);
  printf("  inuse: %d, inbuffercache: %d start: %d end: %d flags: %d\n",
	 p->inuse, p->inbuffercache, p->writestart, p->writeend,p->flags);
  printf("  ts: %ld %ld, mtime: %d %ld\n",
	 p->timestamp.tv_sec, p->timestamp.tv_usec,
	 p->sb.st_mtime, p->sb.st_mtimensec);
}

/* used for pr_filp */
static void 
pr2_nfsc(nfsc_p p) {
  if (p->inuse) printf("**"); else printf("* ");
  printf("(%d) nfsce: %p refcnt: %d dev: %d ino: %d\n",
	 NFSCENUM(p),p,p->refcnt, p->dev,p->sb.st_ino);
//  printf("  fence1: %08x == %08x, fence2: %08x == %08x\n",
//	 p->fence1, NFSCACHE_FENCE1,
//	 p->fence2, NFSCACHE_FENCE2);
  printf("  inuse: %d, inbuffercache: %d start: %d end: %d flags: %d\n",
	 p->inuse, p->inbuffercache, p->writestart, p->writeend, p->flags);
  printf("  ts: %ld %ld, mtime: %d %ld\n",
	 p->timestamp.tv_sec, p->timestamp.tv_usec,
	 p->sb.st_mtime, p->sb.st_mtimensec);
}

void 
nfs_debug(int verbose) {
  nfsc_p p;
  int i;
  extern void pr_server(struct generic_server *serverp);
#if 0
  printf("NFS SHARED DATA\n");
#endif
  for (i = 0; i < NR_NFS_SERVERS; i++) {
    printf("inuse: %d, server pointer: %p\n",
	   FD_ISSET(i,&nfs_shared_data->inuse),
	   &nfs_shared_data->server[i]);
  }
  
  printf("Dumping nfsce table (sz %d st: %p end: %p):\n",
	 NFSATTRCACHESZ,
	 nfs_shared_data->entries,
	 nfs_shared_data->entries + NFSATTRCACHESZ);

  printf("Hint_e not currently used: %p\n",nfs_shared_data->hint_e);
  for (p = nfs_shared_data->entries;
       p < nfs_shared_data->entries + NFSATTRCACHESZ ; 
       p++) {
    if (p->sb.st_ino || p->dev) 
      if (verbose) 
	pr2_nfsc(p);
      else 
	if (p->inuse) pr2_nfsc(p);
  }
  p = nfs_shared_data->entries;
  printf("Server pointer: %p\n",p->fh.server);
  pr_server(p->fh.server);
  
  
}

void 
kpr_nfsc(nfsc_p p) {
  if (p->inuse) kprintf("*");
  kprintf("*nfsce: %p refcnt: %d dev: %d ino: %d\n",
	 p,p->refcnt, p->dev,p->sb.st_ino);
//  kprintf("  fence1: %08x == %08x, fence2: %08x == %08x\n",
//	 p->fence1, NFSCACHE_FENCE1,
//	 p->fence2, NFSCACHE_FENCE2);
  kprintf("  inuse: %d, inbuffercache: %d start: %d end: %d flags: %d\n",
	 p->inuse, p->inbuffercache, p->writestart, p->writeend,p->flags);
  kprintf("  ts: %ld %ld, mtime: %ld %ld\n",
	 p->timestamp.tv_sec, p->timestamp.tv_usec,
	 (long int)p->sb.st_mtime, p->sb.st_mtimensec);
}


void 
print_statbuf(char *f, struct stat *statbuf) {
    printf("file: %s
st_dev:   0x%-8x st_ino:    %-6d     st_mode: 0x%4x st_nlink: %d
st_rdev:  0x%-8x st_uid:    %-6d     st_gid:      %-6d    
st_atime: 0x%-8x st_mtime:  0x%-8x st_ctime:    0x%-8x
st_size:  %-8qd   st_blocks: %-8qd   st_blksize: %5d\n
",
f,
statbuf->st_dev,
statbuf->st_ino,
statbuf->st_mode,
statbuf->st_nlink,
statbuf->st_rdev,
statbuf->st_uid,
statbuf->st_gid,
statbuf->st_atime,
statbuf->st_mtime,
statbuf->st_ctime,
statbuf->st_size,
statbuf->st_blocks,
(int)statbuf->st_blksize);

}  


/* used for debugging of RPC calls */
/* print_rpc[2] prints memory */

void 
print_rpc(int *start, int *end)
{
  int *tmpi;
  char *tmp;
  int i;
  int len;

  tmp = (char *) start;
  tmpi = start;
  len = ((char *) end) - ((char *) start);
  printf ("print_rpc: beg: %d end: %d len: %d\n",(int)start,(int)end,len);
#define PRINTA(c) (isprint(c) ? c : '-')
#define PRINTB(c) ((unsigned char)c)

  for (i = 0 ; i < len; i+=4,tmp += 4, tmpi++)
    {
      char *c = (char *)tmpi;
      printf("[%d] %08x %08x %1c %1c %1c %1c %3u %3u %3u %3u\n",i,
	     *tmpi,(int)htonl(*tmpi),
	     PRINTA(c[0]),
	     PRINTA(c[1]),
	     PRINTA(c[2]),
	     PRINTA(c[3]),

	     PRINTB(c[0]),
	     PRINTB(c[1]),
	     PRINTB(c[2]),
	     PRINTB(c[3])
	     );
    }
  printf("\n");
}

void 
print_rpc2(int *start, int length) {
  int *end;
  end = start + length / sizeof(int *);
  print_rpc(start,end);
}
