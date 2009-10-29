
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

#ifndef _NFS_STRUCT_H_
#define _NFS_STRUCT_H_

#include "nfs_rpc.h"		/* for generic_server */

#include <sys/types.h>		/* for fd_set */
#include <sys/time.h>
#include <sys/stat.h>

#define NR_NFS_SERVERS 8	/* number of mounted nfs servers */

/* if NO_WRITE_SHARING is defined, the attributes are updated after every write,
 otherwise, the attributes are invalidated after each write
 */
//#define NO_WRITE_SHARING	

#define NFSCE_NEVERRELEASE 0x02
#define NFSCE_HASBLOCKINBC 0x04
#define NFSCE_WILLBEGONE 0x08	/* unlink while open */

struct nfsc {
#define NFSCACHE_FENCE1 0xfecce1b1
#define NFSCACHE_FENCE2 0xfecce2b2
  int fence1;
  int flags;
  short refcnt;
  short inuse;
  int dev;
  struct timeval timestamp;
  struct nfs_fh fh;
  struct stat sb;
  int inbuffercache;
  int writestart;
  int n_dirsize;		/* for cached dirents */
  int writeend;
  int fence2;
};
typedef struct nfsc nfsc_t;
typedef struct nfsc *nfsc_p;

#define nfsc_copy_flags(e,eflags)    (e)->flags = (eflags)
#define nfsc_or_flags(e,eflags)      (e)->flags |= (eflags)
#define nfsc_get_flags(e)            (e)->flags
#define nfsc_get_writestart(e)       (e)->writestart
#define nfsc_get_writeend(e)         (e)->writeend
#define nfsc_set_writestart(e,v)     (e)->writestart = (v)
#define nfsc_set_writeend(e,v)       (e)->writeend = (v)
#define nfsc_set_refcnt(e,c)         (e)->refcnt = (c)
#define nfsc_get_refcnt(e)           (e)->refcnt
#define nfsc_inc_refcnt(e)           (e)->refcnt++
#define nfsc_dec_refcnt(e)           (e)->refcnt--

#define nfsc_is_inuse(e)             ((e)->inuse == 1)
#define nfsc_set_inuse(e)            (e)->inuse = 1
#define nfsc_clear_inuse(e)          (e)->inuse = 0
#define nfsc_set_dev(e,d)            (e)->dev = (d)
#define nfsc_set_timestamp(e,ts)     (e)->timestamp = (ts)
#define nfsc_set_sb(e,s)             (e)->sb = (s)
#define nfsc_copyout_sb(e,s)         (s) = (e)->sb
/* check the dirty end */
#define nfsc_get_size(e)             \
   (((e)->writeend > (e)->sb.st_size) ? \
    ((e)->sb.st_size = (e)->writeend) : \
     (e)->sb.st_size)
#define nfsc_set_size(e,v)           (e)->sb.st_size = (v)
#define nfsc_get_mode(e)             (e)->sb.st_mode
#define nfsc_get_fhandle(e)          (&(e)->fh)
#define nfsc_set_fhandle(e,f)        (e)->fh = (f)
#define nfsc_get_mtime(e)            (e)->sb.st_mtime
#define nfsc_get_mtimespec(e)        (e)->sb.st_mtimespec
#define nfsc_get_mtimensec(e)        (e)->sb.st_mtimensec
#define nfsc_get_ts_sec(e)           (e)->timestamp.tv_sec
#define nfsc_get_ts_usec(e)          (e)->timestamp.tv_usec

#define nfsc_ts_iszero(e)              \
     ((e)->timestamp.tv_sec == 0 && (e)->timestamp.tv_usec == 0)
#define nfsc_ts_zero(e)              \
     ((e)->timestamp.tv_sec = 0,(e)->timestamp.tv_usec = 0)

#define NFSATTRCACHESZ           1024	/* number of entries */
#define NFS_LOOKUP_CACHE_SZ  (8*4096)   /* in bytes */

#define NFSMAXOFFSET        (256*1024*1024) /* has to fit in 32 bits */

#define NFS_DIRBLKSZ              512	/* alignment of nfs_getdirentries */

#define NFSFILEMIN         10   /* number of seconds we are ok */
#define NFSDIRMIN           3	/* number of seconds we are ok */
#define NFSLOOKUPMIN        5	/* refresh attributes via lookup */
#define NFSSTATMIN          5	/* refresh attributes via lookup */
#define NFSNCNEGATIVEVALID 10	/* number of seconds name cache
				   negative entries are valid */
#define NFS_CAP 0		/* just a reminder that there is a cap here */

#define GETNFSCEINO(e) ((e)->sb.st_ino)
#define GETNFSCEAT(e) ((e)->sb.st_atime)
#define GETNFSCEDEV(e) (FHGETDEV((&(e)->fh)))

/* nfs block top 32 bits for inode, and bottom 32 bits for pageno */
#define NFSPGSZ 4096
static inline u_quad_t NFSBCBLOCK(int i,int o) {
     return (INT2QUAD (i, o));
   }
static inline int NFSBCBLOCK2I(u_quad_t b) {
  return (QUAD2INT_HIGH (b));
}
static inline int NFSBCBLOCK2O(u_quad_t b) {
  return (QUAD2INT_LOW (b));
}


#define FILLXNAME(dev, xname) {\
    (xname)->xa_dev = dev; (xname)->xa_name = 0;\
}


/* says if an entry returned from nfsc_get is old or new. */
#define NFSCEOLDENTRY(e) ((e)->timestamp.tv_sec || (e)->timestamp.tv_usec)
#define NFSCENEWENTRY(e) (!(NFSCEOLDENTRY(e)))

#define NFSCENUM(e) ((e) - nfs_shared_data->entries)

#define nfsc_settimestamp(p) gettimeofday(&p->timestamp, (void *)0)
#define NFSFILP_settimestamp(filp) gettimeofday(((nfsld_p)filp->data)->ts, (void *)0)


struct nfs_shared_data {
  unsigned int xid;
  fd_set inuse;
  struct generic_server server[NR_NFS_SERVERS];
  nfsc_p hint_e;		/* for by the allocator routine */
  nfsc_t entries[NFSATTRCACHESZ];
};
extern struct nfs_shared_data * const nfs_shared_data;

/* this goes inside the filp local data */
typedef struct nfs_local_data {
  nfsc_p e;
} nfsld_t, *nfsld_p;

#define GETNFSCE(filp) (((nfsld_p)((filp)->data))->e)
#define GETFHANDLE(filp)  nfsc_get_fhandle(GETNFSCE(filp))

/* bc stuff */
#define NFS_CHECK_PAGE 0x001
extern char *
ALLOCATE_BC_PAGE (void);
extern void 
DEALLOCATE_MEMORY_BLOCK (char *ptr, int size);
extern void 
nfs_bc_insert (char *addr, int dev, int ino, int offset);

/* nfs_cache.c */
extern nfsc_p 
nfsc_get(int dev, int ino);
extern void
nfsc_put(nfsc_p p);
extern int
nfsc_checkchange(nfsc_p e, int timeout);

/* for future readahead/writebehind service */
extern int 
nfs_get_nfscd_envid(void);

/* used by nfs_close and inside nfs_rw_new.c */
extern int 
nfsc_sync(nfsc_p);
inline int
nfs_stat_fh(struct nfs_fh *fhandle, nfsc_p e);
struct stat;
inline void 
nfs_fill_stat(struct nfs_fattr *temp_fattr, nfsc_p e);

/* flush routines */

#define NFSC_FLUSH_REGION_ALL 1
#define NFSC_FLUSH_REGION_ALIGN 2 /* leaves the closest aligned blocks */
#define NFSFLUSHALL -1
extern void 
nfs_flush_bc(int dev, int ino, int pageno);

#define nfs_flush_nfsce(e) ({												   \
  int dev,ino;														   \
  assert((void *)(e) != NULL);												   \
  assert( ((void *)((struct nfs_fh *)((nfsc_get_fhandle(e))))->server) != NULL);					   \
  dev = GETNFSCEDEV((e));												   \
  ino = GETNFSCEINO((e));												   \
  e->n_dirsize = 0;													   \
  nfs_flush_bc(dev,ino, NFSFLUSHALL);											   \
})

static inline void 
nfs_flush_nfsce_page(nfsc_p e,int p) {			
  int dev,ino;					
  assert((e) != NULL); 			  
  dev = GETNFSCEDEV((e));			
  ino = GETNFSCEINO((e));			
  nfs_flush_bc(dev,ino, p); 
}

#define nfs_flush_filp(filp) nfs_flush_nfsce(((GETNFSCE(filp))))

static inline int 
nfsc_neq_mtime(nfsc_p e, struct nfs_fattr *fattr) {
  return (((time_t) fattr->mtime.seconds != e->sb.st_mtime ||
	(time_t) fattr->mtime.useconds * 1000 != e->sb.st_mtimensec));
}

extern void 
nfsc_init(nfsc_p cachep, int sz);

/* functions used by nfs_read/write_new and nfs_getdirentries_new */
extern struct bc_entry *
nfs_new_bc_insert (char *addr, int dev, u_quad_t block);
extern char *
new_getbcmapping(struct bc_entry *bc_entry);

struct bc_entry;
char *
getbcmapping(struct bc_entry *bc_entry);

/* functions used for debugging exported by nfs_debug.c */
extern void 
pr_nfsc(nfsc_p p);
extern void 
kpr_nfsc(nfsc_p p);
extern void 
nfs_debug(int verbose);
extern void 
print_statbuf(char *f, struct stat *statbuf);


/* functions exported by nfs_name_cache.c */
int 
nfs_cache_init(void);
int
nfs_cache_lookup(int dev,int ino,const char *name, nfsc_p *p);
void 
nfs_cache_remove_target(struct file *filp);
void 
nfs_cache_remove(struct file *filp, const char *name);
void 
nfs_cache_add(nfsc_p d, char *name, nfsc_p e);
void inline
nfs_cache_add_devino(int dev, int ino, char *name, nfsc_p e);

static inline mode_t
make_st_mode(int mode,int type) {
    switch (type) {
    case NFREG:     type = S_IFREG;    break;
    case NFDIR:     type = S_IFDIR;    break;
    case NFBLK:     type = S_IFBLK;    break;
    case NFCHR:     type = S_IFCHR;    break;
    case NFLNK:     type = S_IFLNK;    break;
    case NFSOCK:    type = S_IFSOCK;   break;
    case NFFIFO:    type = S_IFIFO;    break;
    default:			/* make_st_mode must have a valid type */
	kprintf("NFS make_st_mode: bad Type: %d\n", type);
	type = 0;
	//assert(0);
    }
    return ((mode & ~S_IFMT) | (type & S_IFMT));
}

#endif /* _NFS_STRUCT_H_ */
