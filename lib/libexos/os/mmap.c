
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

/* XXX -- todo: figure out semantics of ppn_is_dirty, do batched pte
   manipulations, added async version flag, implement sub functions */

/* map files or devices into memory */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <xok/sysinfo.h>
#include <xok/sys_ucall.h>
#include <xok/kerrno.h>
#include <xok/bc.h>
#include <fd/proc.h>		/* for bmap.h */
#include <exos/ubc.h>
#include <assert.h>
#include <errno.h>
#include <exos/callcount.h>
#include <exos/mallocs.h>
#include <exos/mregion.h>
#include <exos/vm.h>
#include <exos/vm-layout.h>
#include <exos/process.h>
#include <exos/critical.h>
#include <exos/uwk.h>
#include <exos/cap.h>
#include <fd/nfs/nfs_proc.h>	/* for nfs_bmap_read */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct Mmap {
  void *mmap_addr;
  size_t mmap_len;
  int mmap_prot;
  int mmap_flags;
  u_int mmap_dev;
  off_t mmap_offset;
  struct file *mmap_filp;
  
  LIST_ENTRY(Mmap) mmap_link;
};

struct mmap_ustruct {
  struct mregion_ustruct mru;
  struct mregion_ustruct *oldmru;
  struct Mmap m;
};

LIST_HEAD (Mmap_list, Mmap);
static struct Mmap_list mmap_list;

int mmap_exec(u_int k, int envid, int execonly);
int mmap_fork(u_int k, int envid, int NewPid);
void mmap_exit(void *arg);

static u_int MMAP_TEMP_REGION = 0;

static int mmap_inited = 0;
static int mmap_init()
{
  MMAP_TEMP_REGION = (u_int)exos_pinned_malloc(NBPG*2);  
  if (MMAP_TEMP_REGION == (uint)NULL) return -1;
  LIST_INIT (&mmap_list);
  if (OnExec(mmap_exec) ||
      OnFork(mmap_fork) ||
      ExosExitHandler(mmap_exit, NULL))
    return -1;
  mmap_inited = 1;
  return 0;
}

static inline void close_filp(struct file *filp) {
  int fd;

  for (fd = NR_OPEN - 1; fd >= 0; fd--)
    if (__current->fd[fd] == NULL) {
      __current->fd[fd] = filp;
      break;
    }
  assert(fd >= 0);
  __current->cloexec_flag[fd] = 0;
  lock_filp(filp);
  filp_refcount_inc(filp);
  unlock_filp(filp);
  close(fd);
}

caddr_t __mmap (void *addr, size_t len, int prot, int flags, int fd, 
		off_t offset, u_int ke, int envid) {
  Pte pte = PG_U | PG_P;
  u_quad_t pblock;
  struct stat sb;
  u_int vp = 0;
  dev_t dev;
  int ret = 0;
  u_int uaddr = (u_int )addr;
  u_int seq = 0;
  int done;
  struct bc_entry *b;
  off_t block_offset;

  block_offset = (offset & PGMASK);
  offset &= ~PGMASK;
  len += block_offset;

  if ((flags & MAP_PRIVATE) && (flags & MAP_COPY))
    flags &= ~MAP_PRIVATE;
  /* figure out which pte bits we want to set for the pages in this segment */
  if (prot & PROT_WRITE)
    if (flags & MAP_PRIVATE)
      pte |= PG_COW;
    else
      pte |= PG_W;
  else
    pte |= PG_RO;

  if (flags & MAP_SHARED) pte |= PG_SHARED;

  /* XXX -- need to check access on fd */
  
  /* deal with the address they want to map segment at */

  if (uaddr == (uint)NULL) {
    uaddr = (u_int)__malloc(len);
    assert (!(uaddr & PGMASK));
  } else {
    uaddr = uaddr & ~PGMASK;
  }

  /* get the device that this fd refers to */
  if (fstat (fd, &sb) < 0) {
    errno = EINVAL;
    return (caddr_t )-1;
  }
  dev = sb.st_dev;

  /* make sure all the blocks we're mapping are in the cache (if not
     we read them in) and map them in */

  for (vp = 0; vp < len;) {
    struct Xn_name *xn;
    struct Xn_name xn_nfs;

    /* get the largest extent that starts at this offset */
    if (bmap (fd, &pblock, offset, &seq) < 0) {
      errno = EBADF;
      return (caddr_t )-1;
    }

    /* can not close the race between a bc lookup and attempts to   */
    /* map the associated page (or read it in), so simply do things */
    /* optimisticly and repeat them if necessary.                   */
  __mmap_retryMapPage:
    /* check if the block is in the buffer cache */
    while (!(b = __bc_lookup64 (dev, pblock))) {
      if (dev >= 0) {
	/* disk device */
        int count = 1;
	done = 0;
        assert (seq >= 0);
	/* _exos_bc_read_and_insert returns -E_EXISTS if *any* of the */
	/* requested blocks is in the cache...                      */
        while ((count <= seq) && (count < 16) &&
	       (!__bc_lookup64 (dev, (pblock+count)))) {
	  count++;
        }
	ret = _exos_bc_read_and_insert (dev, (unsigned int)pblock, count,
					&done);
        if (ret == -E_EXISTS) {
	  continue;
        }
	if (ret < 0) {
	  kprintf ("_exos_bc_read_and_insert in mmap returned %d\n", ret);
	  panic ("mmap: error reading in block\n");
	}
	/* sleep until request is completed... */
        wk_waitfor_value_neq (&done, 0, 0);
      } else {
	/* nfs device */
	
	if (nfs_bmap_read (fd, pblock) < 0) {
	  panic ("mmap: error reading block from nfs\n");
	}
      }
    }

    if (b->buf_dev > MAX_DISKS) {
      xn_nfs.xa_dev = b->buf_dev;
      xn_nfs.xa_name = 0;
      xn = &xn_nfs;
    } else {
      xn = &__sysinfo.si_pxn[b->buf_dev];
    }

    if (flags & MAP_COPY) {
      int ret;

      ret = _exos_self_insert_pte (0, (b->buf_ppn << PGSHIFT) |
				    PG_P | PG_U | PG_W, MMAP_TEMP_REGION,
				    ESIP_DONTPAGE, xn);
      if (ret < 0) {
	kprintf ("mmap: ret = %d\n", ret);
	assert (0);
      }
      ret = _exos_self_insert_pte (0, PG_P | PG_U | PG_W,
				    MMAP_TEMP_REGION + NBPG,
				    ESIP_DONTPAGE, NULL);
      if (ret < 0) {
	kprintf ("mmap (2nd): ret = %d\n", ret);
	assert (0);
      }      
      if (b->buf_state & BC_COMING_IN)
	wk_waitfor_value_neq(&b->buf_state, BC_VALID | BC_COMING_IN, 0);
      bcopy((void*)MMAP_TEMP_REGION, (void*)(MMAP_TEMP_REGION + NBPG), NBPG);
      assert(_exos_insert_pte (0, (vpt[PGNO(MMAP_TEMP_REGION + NBPG)] & ~PGMASK)
			       | pte | PG_D, uaddr + vp, ke, envid, 0, NULL) >= 0);
      assert(_exos_self_unmap_page (0, MMAP_TEMP_REGION) >= 0);
      assert(_exos_self_unmap_page (0, MMAP_TEMP_REGION + NBPG) >= 0);
    } else {
      ret = sys_bc_buffer_map (xn, CAP_ROOT, (b->buf_ppn << PGSHIFT) | pte, uaddr + vp,
			       ke, envid);
      if (b->buf_state & BC_COMING_IN)
	wk_waitfor_value_neq(&b->buf_state, BC_VALID | BC_COMING_IN, 0);
    }

    /* recheck that bc entry is still what we want */
    if (b == __bc_lookup64 (dev, pblock)) {
      assert (ret >= 0);
    } else {
      goto __mmap_retryMapPage;
    }
    
    offset += NBPG;
    vp += NBPG;
  }
  
  return (caddr_t )uaddr + block_offset;
}

static inline int mmap_iszerodev(dev)
{
  int	mem_no = 2; 	/* major device number of memory special file */
  
  return major(dev) == mem_no && (minor(dev) < 2 || minor(dev) == 14);
}

/* XXX currently if you mmap a file with a non page-aligned length, what you
   write past what you wanted to mmap will be written to disk (though not
   reflected in the size meta data on the disk, which is good).  It should
   zero it first to be correct. */
static int mmap_fault_handler(struct mregion_ustruct *mru, void *faddr,
			      unsigned int errcode) {
  struct Mmap *m = &(((struct mmap_ustruct*)mru)->m);
  u_int va = (u_int)faddr;
  Pte pte = PG_U | PG_P; /* new page should be present and user space */
  struct Xn_name *xn;
  struct Xn_name xn_nfs;

  /* if it's a write to a page that's not mapped writable then return */
  if ((errcode & FEC_WR) && !(m->mmap_prot & PROT_WRITE)) return 0;

  /* if writable requested... */
  if (m->mmap_prot & PROT_WRITE) pte |= PG_W;

  /* if shared requested... */
  if (m->mmap_flags & MAP_SHARED) pte |= PG_SHARED;


  /* if reading a page that's not present but is mapped private from a file
     then mark it copy-on-write so that it will reflect changes as long as
     possible (must be mapped writable as well) */
  if (!(errcode & FEC_WR) && ((m->mmap_flags &
			       (MAP_PRIVATE | MAP_FILE)) ==
			      (MAP_PRIVATE | MAP_FILE)) &&
      (pte & PG_W)) {
    pte |= PG_COW;
    pte &= ~PG_W;
  }

  /* if mapped anonymous... */
  if (m->mmap_flags & MAP_ANON) {
    /* currently maps a free page and zero's it */
    assert(_exos_self_insert_pte(0, pte, PGROUNDDOWN(va), 0, NULL) == 0);
    bzero((void*)PGROUNDDOWN(va), NBPG);

    return 1;
  }
  else { /* if mapping from a file */
    u_int seq;
    u_quad_t pblock;
    int done = 0, ret, fd;
    struct bc_entry *b;

    /* find a free file descriptor to use with the file pointer during
       the fault */
    for (fd = NR_OPEN - 1; fd >= 0; fd--)
      if (__current->fd[fd] == NULL) {
	__current->fd[fd] = m->mmap_filp;
	break;
      }
    assert (fd >= 0);

    /* if fault is from non-mapped page... */
    if (!(errcode & FEC_PR)) {
      /* map a page from the file */
      ret = bmap(fd, &pblock, m->mmap_offset + PGROUNDDOWN(va) -
		 (u_int)m->mmap_addr, &seq);
      if (ret == -EINVAL && !(m->mmap_flags & MAP_NOEXTEND)) {
	/* XXX File extension not possible for ExOS */
	assert(0);
      } else
	assert(ret == 0);
      assert(seq >= 0);
    mmap_retryMapPage:
      /* check if the block is in the buffer cache */
      while (!(b = __bc_lookup64(m->mmap_dev, pblock))) {
	if ((int)m->mmap_dev >= 0) {
	  /* disk device */
	  int count = 1;
	  /* _exos_bc_read_and_insert returns -E_EXISTS if *any* of the 
	     requested blocks are in the cache... */
	  /* read in up to 64k at a time */
	  while ((count <= seq) && (count < 16) &&
		 (!__bc_lookup64 (m->mmap_dev, (pblock+count)))) {
	    count++;
	  }
	  ret = _exos_bc_read_and_insert(m->mmap_dev, (unsigned int) pblock,
					 count, &done);
	  if (ret == 0)
	    /* sleep until request is completed... */
	    wk_waitfor_value_neq (&done, 0, 0);
	  else if (ret < 0 && ret != -E_EXISTS) {
	    kprintf ("_exos_bc_read_and_insert in mmap returned %d\n", ret);
	    panic ("mmap: error reading in block\n");
	  }
	} else {
	  /* nfs device */
	  if (nfs_bmap_read(fd, pblock) < 0)
	    panic ("mmap: error reading block from nfs\n");
	}
      }
      /* map the page */

      if (b->buf_dev > MAX_DISKS) {
	xn_nfs.xa_dev = b->buf_dev;
	xn_nfs.xa_name = 0;
	xn = &xn_nfs;
      } else {
	xn = &__sysinfo.si_pxn[b->buf_dev];
      }

      ret = _exos_self_insert_pte(0, (b->buf_ppn << PGSHIFT) | pte,
				  ((m->mmap_flags & MAP_PRIVATE) &&
				   (errcode & FEC_WR)) ?
				  MMAP_TEMP_REGION : PGROUNDDOWN(va),
				  ESIP_MMAPED, xn);
      /* make sure the page is completely read in */
      if (b->buf_state & BC_COMING_IN)
	wk_waitfor_value_neq(&b->buf_state, BC_VALID | BC_COMING_IN, 0);
      /* recheck that bc entry is still what we want */
      if (b == __bc_lookup64(m->mmap_dev, pblock)) {
	if (ret < 0) {
	  kprintf ("mmap: ret = %d\n", ret);
	  kprintf ("mmap: b->buf_dev = %d\n", b->buf_dev);
	  assert (0);
	}
      }
      else
	goto mmap_retryMapPage;

      /* if writing to a private page, then make a copy */
      if ((m->mmap_flags & MAP_PRIVATE) && (errcode & FEC_WR)) {
	assert(_exos_self_insert_pte(0, PG_P | PG_U | PG_W,
				     PGROUNDDOWN(va), 0, NULL) == 0);
	bcopy((void*)MMAP_TEMP_REGION, (void*)PGROUNDDOWN(va), NBPG);
	assert(_exos_self_unmap_page(0, MMAP_TEMP_REGION) == 0);
      }
    } else if ((m->mmap_flags & MAP_PRIVATE) && (errcode & FEC_WR) &&
	       (m->mmap_prot & PROT_WRITE)) {
      /* if fault is from a mapped page, but it needs copying... */
      /* perform cow */
      assert(_exos_self_insert_pte(0, PG_P | PG_U | PG_W,
				   MMAP_TEMP_REGION, ESIP_DONTPAGE, NULL) == 0);
      bcopy((void*)PGROUNDDOWN(va), (void*)MMAP_TEMP_REGION, NBPG);
      assert(_exos_self_insert_pte(0, vpt[PGNO(MMAP_TEMP_REGION)],
				   PGROUNDDOWN(va), 0, NULL) == 0);
      assert(_exos_self_unmap_page(0, MMAP_TEMP_REGION) == 0);
    } else { /* trying to write to a page that's mmap'd RO
		or read from system page???... */
      __current->fd[fd] = NULL;
      return 0;
    }

    /* free the file descriptor */
    __current->fd[fd] = NULL;
    return 1;
  }
}

void *mmap (void *addr, size_t len, int prot, int flags, int fd,
	    off_t offset)
{

  u_int pageoff;
  caddr_t ret;
  off_t pos = offset;
  size_t size = len;
  struct Mmap *m;
  struct stat sb;
  struct file *filp;
  struct mmap_ustruct *mus;

  OSCALLENTER(OSCALL_mmap);
  if (!mmap_inited) mmap_init();

  /* if given a bad fd then return */
  if (fd != -1 && fstat (fd, &sb) < 0) {
    errno = EINVAL;
    OSCALLEXIT(OSCALL_mmap);
    return (caddr_t )-1;
  }

  if ((flags & MAP_COPY) && (flags & MAP_ANON)) flags &= ~MAP_COPY;

  /* OpenBSD 2.1 code */
  /*
   * Align the file position to a page boundary,
   * and save its page offset component.
   */
  pageoff = (pos & PGMASK);
  pos -= pageoff;

  /* Adjust size for rounding (on both ends). */
  size += pageoff;	/* low end... */
  size = PGROUNDUP(size); /* hi end */

  /* Do not allow mappings that cause address wrap... */
  if ((ssize_t)size < 0) {
    errno = EINVAL;
    OSCALLEXIT(OSCALL_mmap);
    return (caddr_t)-1;
  }

  /*
   * Check for illegal addresses.  Watch out for address wrap...
   */
  if (flags & MAP_FIXED) {
    /*
     * The specified address must have the same remainder
     * as the file offset taken modulo NBPG, so it
     * should be aligned after adjustment by pageoff.
     */
    addr -= pageoff;
    if ((u_int)addr & PGMASK) {
      errno = EINVAL;
      OSCALLEXIT(OSCALL_mmap);
      return (caddr_t)-1;
    }
    /* Address range must be all in user VM space. */
    if (UTOP > 0 && (u_int)addr + size > UTOP) {
      errno = EINVAL;
      OSCALLEXIT(OSCALL_mmap);
      return (caddr_t)-1;
    }
    if ((u_int)addr > (u_int)addr + size) {
      errno = EINVAL;
      OSCALLEXIT(OSCALL_mmap);
      return (caddr_t)-1;
    }
  }

  if ((flags & MAP_ANON) == 0) {
    if (fd < 0 || fd > NR_OPEN || __current->fd[fd] == NULL) {
      errno = EBADF;
      OSCALLEXIT(OSCALL_mmap);
      return (caddr_t)-1;
    }

    /*
     * XXX hack to handle use of /dev/zero to map anon
     * memory (ala SunOS).
     */
    if (S_ISCHR(__current->fd[fd]->f_mode) &&
	mmap_iszerodev(__current->fd[fd]->f_dev)) {
      flags |= MAP_ANON;
      goto is_anon;
    }

    /*
     * Only files and cdevs are mappable, and cdevs does not
     * provide private mappings of any kind.
     */
    if (!S_ISREG(__current->fd[fd]->f_mode) &&
	(!S_ISCHR(__current->fd[fd]->f_mode) ||
	 (flags & (MAP_PRIVATE|MAP_COPY)))) {
      errno = EINVAL;
      OSCALLEXIT(OSCALL_mmap);
      return (caddr_t)-1;
    }

    /*
     * Ensure that file and memory protections are
     * compatible.  Note that we only worry about
     * writability if mapping is shared; in this case,
     * current and max prot are dictated by the open file.
     * XXX use the vnode instead?  Problem is: what
     * credentials do we use for determination?
     * What if proc does a setuid?
     */
    if (((__current->fd[fd]->f_flags & O_ACCMODE) == O_WRONLY) &&
	(prot & PROT_READ)) {
      errno = EACCES;
      OSCALLEXIT(OSCALL_mmap);
      return (caddr_t)-1;
    }

    /*
     * If we are sharing potential changes (either via MAP_SHARED
     * or via the implicit sharing of character device mappings),
     * and we are trying to get write permission although we
     * opened it without asking for it, bail out.
     */
    if (((flags & MAP_SHARED) != 0 || S_ISCHR(__current->fd[fd]->f_mode)) &&
	((__current->fd[fd]->f_flags & O_ACCMODE) == O_RDONLY) &&
	(prot & PROT_WRITE) != 0) {
      errno = EACCES;
      OSCALLEXIT(OSCALL_mmap);
      return (caddr_t)-1;
    }
  } else {
    /*
     * (flags & MAP_ANON) == TRUE
     * Mapping blank space is trivial.
     */
    if (fd != -1) {
      errno = EINVAL;
      OSCALLEXIT(OSCALL_mmap);
      return (caddr_t)-1;
    }
  is_anon:
    pos = 0;
  }

  if (size == 0) {
    OSCALLEXIT(OSCALL_mmap);
    return addr; 
  }

  if (fd >= 0)
    filp = __current->fd[fd];
  else
    filp = NULL;

  if ((flags & MAP_FIXED) == 0) {
    addr = __malloc(size);
    if (addr == NULL) {
      __free(addr);
      errno = ENOMEM;
      OSCALLEXIT(OSCALL_mmap);
      return (caddr_t)-1;
    }
  }

  mus = exos_pinned_malloc(sizeof(*mus));
  if (mus == NULL) {
    if ((flags & MAP_FIXED) == 0) __free(addr);
    errno = ENOMEM;
    OSCALLEXIT(OSCALL_mmap);
    return (caddr_t)-1;
  }

  m = &(mus->m);
  m->mmap_addr = addr;
  m->mmap_len = size;
  m->mmap_prot = prot;
  m->mmap_flags = flags;
  m->mmap_offset = pos;
  m->mmap_filp = filp;
  m->mmap_dev = ((fd != -1) ? sb.st_dev : 0);
  LIST_INSERT_HEAD (&mmap_list, m, mmap_link);

  mus->mru.handler = mmap_fault_handler;
  mus->oldmru = mregion_get_ustruct(addr); /* XXX - check return value */

  if (__vm_free_region((u_int)addr, size, 0) < 0 ||
      mregion_alloc(addr, size, (struct mregion_ustruct*)mus) != 0) {
    if ((flags & MAP_FIXED) == 0) __free(addr);
    exos_pinned_free(mus);
    errno = ENOMEM;
    OSCALLEXIT(OSCALL_mmap);
    return (caddr_t)-1;
  }

  if (filp) {
    lock_filp(filp);
    filp_refcount_inc(filp);
    unlock_filp(filp);
  }

  if (flags & MAP_COPY)
    ret = __mmap(addr, size, prot, flags, fd, pos, 0, __envid);
  else
    ret = addr + pageoff;

  OSCALLEXIT(OSCALL_mmap);
  return ret;
}

static void msync_mark_bc(struct Mmap *m, caddr_t addr, size_t len) {
  u_int va;

  /* XXX */ return;
  for (va = (u_int)addr; va < (u_int)addr + len; va += NBPG)
    if (vpt[PGNO(va)] & PG_D) {
      /* dirty block */
      struct bc_entry *b = __bc_lookup(m->mmap_dev, va - (u_int)m->mmap_addr +
				       m->mmap_offset);

      assert(b);
      if (b->buf_dirty != BUF_DIRTY) {
	assert(sys_bc_set_dirty(b->buf_dev, b->buf_blk, 1) == 0);
      }
    }
}

/* XXX - need to check accessed and dirty bits... */
static void msync_set_modtimes(struct file *filp) {
  int fd;

  /* XXX */ return;
  for (fd = NR_OPEN - 1; fd >= 0; fd--) {
    if (__current->fd[fd] == NULL) {
      __current->fd[fd] = filp;
      assert(futimes(fd, NULL) == 0);
      __current->fd[fd] = NULL;
      return;
    }
  }
  assert(0);
}

int msync(void *addr, size_t len, int flags) {
  struct mmap_ustruct *mus;
  struct Mmap *m;
  void *nextaddr;
  size_t nextlen;

  OSCALLENTER(OSCALL_msync);
  if (((u_int)addr & PGMASK) || len < 0) {
    errno = EINVAL;
    OSCALLEXIT(OSCALL_msync);
    return -1;
  }
  if (len == 0) {
    OSCALLEXIT(OSCALL_msync);
    return 0;
  }

  nextlen = len;

  do {
    mus = (struct mmap_ustruct *)mregion_get_ustruct(addr);
    if (!mus) {
      errno = EINVAL;
      OSCALLEXIT(OSCALL_msync);
      return -1;
    }
    m = &(mus->m);

    if (addr+len > m->mmap_addr+m->mmap_len)
      len -= addr+len - (m->mmap_addr+m->mmap_len);

    msync_mark_bc(m, addr, (u_int)addr + len);
    msync_set_modtimes(m->mmap_filp);

    nextaddr = addr+len;
    nextlen -= len;
    addr = nextaddr;
    len = nextlen;
  } while (len > 0);
  
  OSCALLEXIT(OSCALL_msync);
  return 0;
}

int munmap(void *addr, size_t len) {
  struct Mmap *m;
  struct mmap_ustruct *mus;
  void *nextaddr;
  size_t nextlen;

  OSCALLENTER(OSCALL_munmap);
  /* page-ify */
  len += (((u_int)addr) & PGMASK);
  addr = (void*)PGROUNDDOWN((u_int)addr);
  len = PGROUNDUP(len);
  /* impossible to do what man page says! */
#if 0
  if ((((u_int)addr) & PGMASK) || len < 0) {
    errno = EINVAL;
    OSCALLEXIT(OSCALL_munmap);
    return -1;
  }
#endif
  if (len == 0) {
    OSCALLEXIT(OSCALL_munmap);
    return 0;
  }

  nextlen = len;

  do {
    /* get info on the to-be-freed region */
    mus = (struct mmap_ustruct *)mregion_get_ustruct(addr);
    if (!mus) {
      errno = EINVAL;
      OSCALLEXIT(OSCALL_munmap);
      return -1;
    }
    m = &(mus->m);

    if (addr+len > m->mmap_addr+m->mmap_len)
      len -= addr+len - (m->mmap_addr+m->mmap_len);

    /* something strange, shouldn't happen */
    if (addr >= m->mmap_addr+m->mmap_len ||
	addr+len <= m->mmap_addr) {
      OSCALLEXIT(OSCALL_munmap);
      return 0;
    }

    /* if completely freed */
    if (addr <= m->mmap_addr && len >= m->mmap_len) {
      __vm_free_region((u_int)m->mmap_addr, m->mmap_len,
		       CAP_ROOT); /* XXX - error check */
      __free(m->mmap_addr); /* if wasn't __malloc'd then this will do nothing */
      LIST_REMOVE(m, mmap_link);
      if (m->mmap_filp) {
	lock_filp(m->mmap_filp);
	filp_refcount_dec(m->mmap_filp);
	if (filp_refcount_get(m->mmap_filp) == 0) {
	  unlock_filp(m->mmap_filp);
	  close_filp(m->mmap_filp);
	} else
	  unlock_filp(m->mmap_filp);
      }
      exos_pinned_free(mus);
      /* retore original handler to region */
      if (mregion_alloc(addr, len, mus->oldmru) < 0) {
	errno = EINVAL;
	OSCALLEXIT(OSCALL_munmap);
	return -1;
      }
    } /* if the end is freed */
    else if (addr > m->mmap_addr && addr+len >= m->mmap_addr+m->mmap_len) {
      m->mmap_len = addr-m->mmap_addr;
      __vm_free_region((u_int)addr, len, CAP_ROOT); /* XXX - error check */
      __free2(addr, 0); /* if wasn't __malloc'd then this will do nothing */
      /* retore original handler to region */
      if (mregion_alloc(addr, len, mus->oldmru) < 0) {
	errno = EINVAL;
	OSCALLEXIT(OSCALL_munmap);
	return -1;
      }
    } /* if the beginning is freed */
    else if (addr <= m->mmap_addr && addr+len < m->mmap_addr+m->mmap_len) {
      __vm_free_region((u_int)addr, len, CAP_ROOT); /* XXX - error check */
      __free2(m->mmap_addr, addr+len - m->mmap_addr);
      m->mmap_len = m->mmap_addr+m->mmap_len - (addr+len);
      m->mmap_addr = addr+len;
      /* retore original handler to region */
      if (mregion_alloc(addr, len, mus->oldmru) < 0) {
	errno = EINVAL;
	OSCALLEXIT(OSCALL_munmap);
	return -1;
      }
    } /* if the middle is freed */
    else {
      __vm_free_region((u_int)addr, len, CAP_ROOT); /* XXX - error check */
      /* retore original handler to region */
      if (mregion_alloc(addr, len, mus->oldmru) < 0) {
	errno = EINVAL;
	OSCALLEXIT(OSCALL_munmap);
	return -1;
      }
      assert(0); /* XXX - too much trouble right now */
    }

    nextaddr = addr+len;
    nextlen -= len;
    addr = nextaddr;
    len = nextlen;
  } while (len > 0);

  OSCALLEXIT(OSCALL_munmap);
  return 0;
}

int mmap_exec(u_int k, int envid, int execonly) {
  /* munmap all non-MAP_INHERIT regions */
  struct Mmap *m2, *m = mmap_list.lh_first;

  if (!execonly) return 0;

  while (m)
    if (m->mmap_flags & MAP_INHERIT) {
      /* not implemented - need to set up a region of vm to mmap data */
      assert(0);
      m = m->mmap_link.le_next;
    }
    else {
      m2 = m->mmap_link.le_next;
      assert(msync(m->mmap_addr, m->mmap_len, 0) == 0);
      if (m->mmap_filp) {
	lock_filp(m->mmap_filp);
	filp_refcount_dec(m->mmap_filp);
	if (filp_refcount_get(m->mmap_filp) == 0) {
	  unlock_filp(m->mmap_filp);
	  close_filp(m->mmap_filp);
	} else
	  unlock_filp(m->mmap_filp);
      }
      m = m2;
    }

  return 0;
}

int mmap_fork(u_int k, int envid, int NewPid) {
  /* increase the refcounts */
  struct Mmap *m = mmap_list.lh_first;

  while (m) {
    if (m->mmap_filp) {
      lock_filp(m->mmap_filp);
      filp_refcount_inc(m->mmap_filp);
      unlock_filp(m->mmap_filp);
    }
    m = m->mmap_link.le_next;
  }

  return 0;
}

void mmap_exit(void *arg) {
  /* munmap all regions */
  struct Mmap *m2, *m = mmap_list.lh_first;

  while (m) {
    m2 = m->mmap_link.le_next;
    assert(msync(m->mmap_addr, m->mmap_len, 0) == 0);
    if (m->mmap_filp) {
      lock_filp(m->mmap_filp);
      filp_refcount_dec(m->mmap_filp);
      if (filp_refcount_get(m->mmap_filp) == 0) {
	unlock_filp(m->mmap_filp);
	close_filp(m->mmap_filp);
      } else
	unlock_filp(m->mmap_filp);
    }
    m = m2;
  }
}
