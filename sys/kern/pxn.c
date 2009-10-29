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


#include <xok/pxn.h>
#include <xok/kerrno.h>
#include <xok/env.h>
#include <xok/queue.h>
#include <xok/sys_proto.h>
#include <xok/malloc.h>
#include <xok_include/assert.h>

LIST_HEAD(pxn_list, Pxn);
TAILQ_HEAD(pxn_tailq, Pxn);

/* pxn cache */
static struct pxn_cache_struct {
#define PXN_NUM_QS 256
#define PXN_MASK_QS 0xff
  struct pxn_list pxn_qs[PXN_NUM_QS];	/* a bunch of hash-queues */
} pxn_cache;

static struct pxn_tailq pxn_lru;

/* true if x2 is completely contained within x1 */
static int xtnt_dominates (struct Xn_xtnt *x1, struct Xn_xtnt *x2) {
  return ((x2->xtnt_block >= x1->xtnt_block) && 
          (x2->xtnt_block < (x1->xtnt_block + x1->xtnt_size)) &&
	  ((x2->xtnt_block + x2->xtnt_size) > x1->xtnt_block) &&
          ((x2->xtnt_block + x2->xtnt_size) <= (x1->xtnt_block + x1->xtnt_size)));
}
  
static int find_free_bptr (struct Pxn *p) {
  int i;

  for (i = 0; i < p->pxn_bptr_sz; i++) {
    if (p->pxn_bptrs[i].bptr_type == BPTR_EMPTY) return i;
  }
  return -E_XTNT_SIZE;
}

static int pxn_has_write_caps (struct Pxn *p) {
  /* XXX */
  return 0;
}

/* hash (d,o,s) -> [0..PXN_NUM_QS-1] */
static inline u_int pxn_hash (struct Xn_name *n) {
  return ((n->xa_dev + n->xa_name) & PXN_MASK_QS);
}

static void remember_pxn (struct Pxn *p) {
  LIST_INSERT_HEAD (&pxn_cache.pxn_qs[pxn_hash (&p->pxn_name)], p, pxn_link);
  TAILQ_INSERT_TAIL (&pxn_lru, p, pxn_lru_link);
}

static void forget_pxn (struct Pxn *p) {
  LIST_REMOVE (p, pxn_link);
  TAILQ_REMOVE (&pxn_lru, p, pxn_lru_link);
}

struct Pxn *lookup_pxn (struct Xn_name *n) {
  struct Pxn *p;

  p = pxn_cache.pxn_qs[pxn_hash (n)].lh_first;
  while (p) {
    if (p->pxn_name.xa_dev == n->xa_dev &&
	p->pxn_name.xa_name == n->xa_name) {
	TAILQ_REMOVE (&pxn_lru, p, pxn_lru_link);
	TAILQ_INSERT_TAIL (&pxn_lru, p, pxn_lru_link);
	return (p);
      }
    p = p->pxn_link.le_next;
  }
  return NULL;
}

static int is_dev_used (u32 dev) {
  struct Pxn *p;
  int i;

  for (i = 0; i < PXN_NUM_QS; i++) {
    p = pxn_cache.pxn_qs[i].lh_first;
    while (p) {
      if (p->pxn_name.xa_dev == dev) {
	return 1;
      }
      p = p->pxn_link.le_next;
    }
  }
  return 0;
}
 
/* check if c gives access to extents in p */
static inline int xtnt_verify_access (struct Pxn *p, cap *c, int access,
				      int *error) {
  int r;

  r = acl_access (c, p->pxn_xtnt_acl, p->pxn_xtnt_acl_max, access);
  if (r < 0) *error = r;
  return (r >= 0);
}

/* check if k gives access to p */
inline int pxn_verify_access (struct Pxn *p, cap *c, int access,
			      int *error) {
  int r;

  r = acl_access (c, p->pxn_pxn_acl, p->pxn_pxn_acl_max, access);
  if (r < 0) *error = r;
  return (r >= 0);
}

/* one-time init function */
void pxn_init () {
  int i;

  TAILQ_INIT (&pxn_lru);
  for (i=0; i < PXN_NUM_QS; i++)
    LIST_INIT (&pxn_cache.pxn_qs[i]);
}

/* allocate an empty pxn
 *
 * The new pxn named n that has an acl with space for pxn_caps caps to
 * guard the pxn itself and an acl of length xtnt_caps for the extents
 * in the pxn. The initial capability for the pxn is c_pxn and the
 * initial capability for the extents is c_xtnt. With space for bptrs
 * bptrs in the pxn.
 *
 */
int pxn_alloc (u8 pxn_caps, u8 xtnt_caps, u16 bptrs, cap *c_pxn, cap *c_xtnt,
	       struct Xn_name *n) {
  struct Pxn *pxn;
  u16 sz;
  int r;
  int i;

  /* see if the requested name already exists.
   *
   * Rules are: if it's a disk pxn, make sure the exact name doesn't
   * already exist. If it's a pseudo-device (dev < 0), then make sure
   * NO other pxn is already using the same device number.
   *
   */
  if (is_pxn_disk_dev(n)) {
    if (lookup_pxn (n)) {
      return -E_EXISTS;
    }
  } else {
    if (is_dev_used (n->xa_dev)) {
      return -E_EXISTS;
    }
  }

  sz = sizeof (struct Pxn) + 
    (xtnt_caps + pxn_caps) * sizeof (cap) + 
    bptrs * sizeof (struct Xn_bptr);
  pxn = (struct Pxn *)malloc (sz);
  if (!pxn) return -E_NO_MEM;

  pxn->pxn_name = *n;
  pxn->pxn_bptr_sz = bptrs;
  pxn->pxn_xtnt_acl_max = xtnt_caps;
  pxn->pxn_pxn_acl_max = pxn_caps;
  pxn->pxn_refcnt = 1;		/* we're creating a refernce */
  pxn->pxn_frozen = 0;

  /* setup pointers to acl's and bptr array -- they immediately follow
     the pxn struct itself */
  pxn->pxn_xtnt_acl = (cap *)((char *)pxn+sizeof (struct Pxn));
  acl_init (pxn->pxn_xtnt_acl, xtnt_caps);
  pxn->pxn_pxn_acl = (cap *)((char *)pxn->pxn_xtnt_acl+xtnt_caps*sizeof (cap));
  acl_init (pxn->pxn_pxn_acl, pxn_caps);
  pxn->pxn_bptrs = (struct Xn_bptr *)((char *)pxn->pxn_pxn_acl + 
				      pxn_caps*sizeof (cap));

  for (i = 0; i < bptrs; i++) {
    pxn->pxn_bptrs[i].bptr_type = BPTR_EMPTY;
  }

  if (acl_add (pxn->pxn_pxn_acl, pxn_caps, c_pxn, &r)) {
    free(pxn);
    return r;
  }
  if (acl_add (pxn->pxn_xtnt_acl, xtnt_caps, c_xtnt, &r)) {
    free(pxn);
    return r;
  }

  remember_pxn (pxn);

  return (0);
}

/* syscall wrapper around pxn_alloc */
int sys_pxn_alloc (u_int sn, u8 pxn_caps, u8 xtnt_caps, u16 bptrs,
		   u_int k_pxn, u_int k_xtnt, struct Xn_name *n) {
  cap c_pxn, c_xtnt;
  struct Xn_name xn_kernel;
  int r;

  copyin (n, &xn_kernel, sizeof (xn_kernel));
  if ((r = env_getcap (curenv, k_pxn, &c_pxn)) < 0) return r;
  if ((r = env_getcap (curenv, k_xtnt, &c_xtnt)) < 0) return r;
  
  return (pxn_alloc (pxn_caps, xtnt_caps, bptrs, &c_pxn, &c_xtnt, &xn_kernel));
}

/* Remove an extent from a pxn and all other extents forged from this extent.
 *
 * Go through all the extents in the specified pxn and remove any that are
 * subsets of the specified extent.
 *
 */
void pxn_remove_xtnt (struct Pxn *p, struct Xn_xtnt *xtnt) {
  struct Xn_xtnt_ref *xr;
  int bptr;
  struct Pxn *pxn_forged_from; /* pxn owning the extent that the extent being
				  removed was forged from */
  struct Xn_xtnt *xtnt_forged_from; /* extent within pxn_forged_from that the
				       extent being removed was forged from */
  int bptr_forged_from;

  /* iterate over all the bptrs in p */
  for (bptr = 0; bptr < p->pxn_bptr_sz; bptr++) {

    /* does this bptr contain an extent? */
    if (p->pxn_bptrs[bptr].bptr_type == BPTR_XTNT) {

      /* does this extent fall under the range of what we're removing? */
      if (xtnt_dominates (xtnt, &p->pxn_bptrs[bptr].bptr_xtnt)) {

	/* remove bptrs that were forged from this one */
	for (xr = p->pxn_bptrs[bptr].bptr_xtnt.xtnt_forged_to.lh_first; xr; 
	     xr = xr->xr_next.le_next) {

	  /* remember, xr->xr_pxn is the name of a pxn that forged an
	     extent from the extent we're removing and xr->xr_bptr is the
	     bptr within xr->xr_pxn that was created. */
	  pxn_remove_xtnt (xr->xr_pxn,
			   &xr->xr_pxn->pxn_bptrs[xr->xr_bptr].bptr_xtnt);
	}

	/* Remove this extent from whoever it was forged from so
	 * they don't try to remove it if they are ever removed
	 * themselves.
         *
	 * Each extent records from which other extent it was forged
	 * from in xtnt_forged_from. This points to the pxn and the
	 * bptr within that pxn that it was forged from.
         *
	 * Using xtnt_forged_from we can find the pxn/bptr that we came
	 * from. Then we go through that pxn/bptr's xtnt_forged_to which
	 * should record the fact that we were forged from it.
	 *
	 */
	pxn_forged_from =
	  p->pxn_bptrs[bptr].bptr_xtnt.xtnt_forged_from.xr_pxn;
	if (pxn_forged_from) {
	  bptr_forged_from =
	    p->pxn_bptrs[bptr].bptr_xtnt.xtnt_forged_from.xr_bptr;
	  xtnt_forged_from =
	    &pxn_forged_from->pxn_bptrs[bptr_forged_from].bptr_xtnt;

	  /* iterate over parent extent's list of who was forged from it */
	  for (xr = xtnt_forged_from->xtnt_forged_to.lh_first; xr; 
	       xr = xr->xr_next.le_next) {
	  
	    /* look for the record of this particular bptr being
	       forged from them. */
	    if (xr->xr_pxn == p && xr->xr_bptr == bptr) {
	      /* remove this record */
	      LIST_REMOVE (xr, xr_next);
	      break;
	    }
	  }

	  /* if we fell off the end of the list then we didn't find
	     ourselves, which we should have */
	  if (!xr) {
	    warn ("pxn_xtnt_remove: didn't find self in parent's forged_to"
		  " list");
	  }
	}

	/* And finally, remove the extent in the current pxn */
	p->pxn_bptrs[bptr].bptr_type = BPTR_EMPTY;
      }
    }
  }
}

/* syscall wrapper around pxn_remove_xtnt */
int sys_pxn_remove_xtnt (u_int sn, struct Xn_name *n, struct Xn_xtnt *xtnt,
			 u_int k) {
  struct Xn_name xn_kernel;
  struct Xn_xtnt xtnt_kernel;
  int r;
  cap c;
  struct Pxn *p;

  copyin (n, &xn_kernel, sizeof (xn_kernel));
  copyin (xtnt, &xtnt_kernel, sizeof (xtnt_kernel));

  p = lookup_pxn(&xn_kernel);
  if (!p) return -E_NOT_FOUND;

  if ((r = env_getcap (curenv, k, &c)) < 0) return r;

  /* make sure we have write access to this pxn */
  if (!pxn_verify_access (p, &c, ACL_W, &r)) return r;

  pxn_remove_xtnt (p, &xtnt_kernel);

  return 0;
}

/* remove references to p2 from p1 */
int pxn_remove_pxn (struct Pxn *p1, struct Pxn *p2, cap *c_p1) {
  int i;
  int r;

  if (! pxn_verify_access (p1, c_p1, ACL_W, &r)) return r;

  for (i = 0; i < p1->pxn_bptr_sz; i++) {
    if (p1->pxn_bptrs[i].bptr_type == BPTR_PXN &&
	p1->pxn_bptrs[i].bptr_pxn == p2) {
      pxn_dealloc (p2);
      p1->pxn_bptrs[i].bptr_type = BPTR_EMPTY;
/* XXX - does this mean there can be more than one reference to a pxn
   per pxn?
*/
    }
  }

  return 0;
}

int sys_pxn_remove_pxn (u_int sn, struct Xn_name *xn1, struct Xn_name *xn2,
			u_int k) {
  /* XXX */
  assert (0);
}

/* deallocate a pxn */
void pxn_dealloc (struct Pxn *p) {
  int i;

  /* only free this pxn if we're the last reference to it */
  if (--p->pxn_refcnt) return;

  /* iterate over all the bptrs freeing whatever they point to */
  for (i = 0; i < p->pxn_bptr_sz; i++) {
    switch (p->pxn_bptrs[i].bptr_type) {
    case BPTR_EMPTY: break;
    case BPTR_XTNT: pxn_remove_xtnt (p, &p->pxn_bptrs[i].bptr_xtnt); break;
    case BPTR_PXN: pxn_dealloc (p->pxn_bptrs[i].bptr_pxn); break;
    default: warn ("pxn_dealloc: bogus bptr type");
    }
  }

  /* and finally actually remove the pxn itself */
  forget_pxn (p);
  free (p);
}

/* syscall wrapper around pxn_deallocate */
int sys_pxn_dealloc (u_int sn, u_int k, struct Xn_name *n) {
  cap c;
  int r;
  struct Xn_name xn_kernel;
  struct Pxn *p;

  if ((r = env_getcap (curenv, k, &c)) < 0) return r;
  copyin (n, &xn_kernel, sizeof (xn_kernel));

  p = lookup_pxn (&xn_kernel);
  if (!p) return -E_NOT_FOUND;

  /* make sure c gives us write access to the capability itself */
  if (!pxn_verify_access (p, &c, ACL_W, &r)) return r;

  pxn_dealloc (p);
  return 0;
}

/* add a capability to either the pxn acl or the extent acl */
int pxn_add_cap (struct Xn_name *n, cap *c_new, cap *c_pxn, int flag) {
  struct Pxn *pxn;
  int r;

  if (!(pxn = lookup_pxn (n))) return -E_NOT_FOUND;
  if (!pxn_verify_access (pxn, c_pxn, ACL_W, &r)) return r;
  if (pxn->pxn_frozen) return -E_XTNT_RDONLY;

  if (flag) {
    if (acl_add (pxn->pxn_pxn_acl, pxn->pxn_pxn_acl_max, c_new, &r))
      return r;
  } else {
    if (acl_add (pxn->pxn_xtnt_acl, pxn->pxn_xtnt_acl_max, c_new, &r))
      return r;
  }

  return 0;
}

/* system call wrapper around pxn_add_cap */
int sys_pxn_add_cap (u_int sn, struct Xn_name *n, u_int k_new, u_int k_pxn,
		     int flag) {
  int r;
  cap cap_pxn, cap_new;
  struct Xn_name xn_kernel;

  if ((r = env_getcap (curenv, k_pxn, &cap_pxn)) < 0) return r;
  if ((r = env_getcap (curenv, k_new, &cap_new)) < 0) return r;
  copyin (n, &xn_kernel, sizeof (xn_kernel));

  return (pxn_add_cap (&xn_kernel, &cap_new, &cap_pxn, flag));
}

/* return the bptr index of an extent in p that dominates xtnt */
static int find_dominating_xtnt (struct Pxn *p, struct Xn_xtnt *xtnt) {
  int i;

  for (i = 0; i < p->pxn_bptr_sz; i++) {
    if (p->pxn_bptrs[i].bptr_type == BPTR_XTNT) {
      if (xtnt_dominates (&p->pxn_bptrs[i].bptr_xtnt, xtnt))
	return i;
    }
  }

  return -E_NOT_FOUND;
}

int pxn_kernel_add_xtnt (struct Xn_name *xn, struct Xn_xtnt *xtnt) {
  int i;
  struct Pxn *p;

  p = lookup_pxn (xn);
  if (!p) return -E_NOT_FOUND;

  i = find_free_bptr (p);
  if (i < 0) return i;

  p->pxn_bptrs[i].bptr_xtnt = *xtnt;
  p->pxn_bptrs[i].bptr_type = BPTR_XTNT;
  LIST_INIT (&p->pxn_bptrs[i].bptr_xtnt.xtnt_forged_to);
  p->pxn_bptrs[i].bptr_xtnt.xtnt_forged_from.xr_pxn = NULL;
  return 0;
}

/* add an extent to dst_pxn if it is contained in src_pxn and we have
   write access to dst_pxn. */
int pxn_add_xtnt (struct Pxn *src_pxn, struct Pxn *dst_pxn, 
		  struct Xn_xtnt *xtnt, cap *c_dst_pxn, cap *c_src_pxn,
		  cap *c_src_xtnt) {

  struct Xn_xtnt_ref *xr;
  int r;
  int src_bptr;
  int i;
  int dst_freeze = 0;
  struct Xn_xtnt *new;

  /* make sure we can write the dst pxn */
  if (!pxn_verify_access (dst_pxn, c_dst_pxn, ACL_W, &r)) return r;

  /* pseudo-devices have relaxed extent addition rules. Basically,
   * we need this so that a user-land process can bootstrap the
   * first pxn.
   *
   * This is safe because the first process to add a particular
   * pseudo-device prevents anyone else from using that same
   * pseudo-device unless they have the right cap. So you don't need
   * to worry about someone else forging pxn's and adding arbitrary
   * extents.
   * 
   */
  if (!is_pxn_disk_dev (&dst_pxn->pxn_name)) {
    assert (src_pxn == NULL);
    return (pxn_kernel_add_xtnt (&dst_pxn->pxn_name, xtnt));
  }
  
  /* make sure we can read the src pxn */
  if (!pxn_verify_access (src_pxn, c_src_pxn, ACL_R, &r)) return r;

  /* make sure this extent exists in src_pxn */
  src_bptr = find_dominating_xtnt (src_pxn, xtnt);
  if (src_bptr < 0) return src_bptr;
  
  /* make sure we can at least read the extent */
  if (!xtnt_verify_access (src_pxn, c_src_xtnt, ACL_R, &r)) return r;

  /* if we can't write the extent we have to remove all write-permissions
     from the pxn and prevent the capabilities from being changed. Otherwise,
     a user could create a new pxn, add an extent the user only has read
     permission on, and then add a capbility the user has write access on
     to the new pxn and thereby get write access to the extent. */
  if (!xtnt_verify_access (src_pxn, c_src_xtnt, ACL_W, &r)) {
    /* XXX - doublecheck: remove or disallow? */
    if (pxn_has_write_caps (dst_pxn)) return -E_XTNT_RDONLY;
    dst_freeze = 1; /* don't commmit freeze until error checking done */
  }
  
  /* ok, so at this point we can add a new extent to dst_pxn and link
     the src pxn and src extent to it so they can revoke it if need be */
  i = find_free_bptr (dst_pxn);
  if (i < 0) return i;

  /* add the extent to dst */
  dst_pxn->pxn_bptrs[i].bptr_type = BPTR_XTNT;
  dst_pxn->pxn_bptrs[i].bptr_xtnt = *xtnt;
  LIST_INIT (&dst_pxn->pxn_bptrs[i].bptr_xtnt.xtnt_forged_to);

  /* add entry in src remembering that a new extent was forged from it */

  /* create a link object for the src_pxn to the dst_pxn/bptr */
  xr = malloc (sizeof (struct Xn_xtnt_ref));
  if (!xr) {
    dst_pxn->pxn_bptrs[i].bptr_type = BPTR_EMPTY;
    return -E_NO_MEM;
  }
  xr->xr_pxn = dst_pxn;
  xr->xr_bptr = i;

  /* and hook it in */
  LIST_INSERT_HEAD (&src_pxn->pxn_bptrs[src_bptr].bptr_xtnt.xtnt_forged_to,
		    xr, xr_next);

  /* remember in dst where this extent was forged from */
  new = &dst_pxn->pxn_bptrs[i].bptr_xtnt;
  new->xtnt_forged_from.xr_pxn = src_pxn;
  new->xtnt_forged_from.xr_bptr = src_bptr;

  /* apply freezing */
  if (dst_freeze) dst_pxn->pxn_frozen = 1;

  return 0;
}

/* system call wrapper around pxn_add_xtnt */
int sys_pxn_add_xtnt (u_int sn, struct Xn_name *src_xn,
		      struct Xn_name *dst_xn, struct Xn_xtnt *xtnt,
		      int access, u_int k_dst_pxn, u_int k_src_pxn,
		      u_int k_src_xtnt) {

  struct Pxn *src_pxn = NULL, *dst_pxn;
  struct Xn_name src_xn_kernel, dst_xn_kernel;
  struct Xn_xtnt xtnt_kernel;
  cap c_dst_pxn, c_src_pxn, c_src_xtnt;
  int r;
  int enforce_pseudo = 0;

  if (src_xn) {
    copyin (src_xn, &src_xn_kernel, sizeof (src_xn_kernel));
  } else {
    enforce_pseudo = 1;
  }

  copyin (dst_xn, &dst_xn_kernel, sizeof (dst_xn_kernel));
  copyin (xtnt, &xtnt_kernel, sizeof (xtnt_kernel));

  if (enforce_pseudo && is_pxn_disk_dev (&dst_xn_kernel))
    return -E_NOT_FOUND;
  if (!enforce_pseudo && !(src_pxn = lookup_pxn (&src_xn_kernel)))
    return -E_NOT_FOUND;
  if (!(dst_pxn = lookup_pxn (&dst_xn_kernel))) return -E_NOT_FOUND;

  if ((r = env_getcap (curenv, k_dst_pxn, &c_dst_pxn))<0) return r;
  if (!enforce_pseudo && (r = env_getcap (curenv, k_src_pxn, &c_src_pxn))<0)
    return r;
  if (!enforce_pseudo && (r = env_getcap (curenv, k_src_xtnt, &c_src_xtnt))<0)
    return r;

  return (pxn_add_xtnt (src_pxn, dst_pxn, &xtnt_kernel, &c_dst_pxn, &c_src_pxn, 
			&c_src_xtnt));
}

int pxn_add_pxn (struct Pxn *dst_pxn, struct Pxn *src_pxn, cap *c_dst,
		 cap *c_src) {
  int i;
  int r;

  /* make sure we can read the src pxn and write the dst pxn */
  if (!pxn_verify_access (src_pxn, c_src, ACL_R, &r)) return r;
  if (!pxn_verify_access (dst_pxn, c_dst, ACL_W, &r)) return r;

  i = find_free_bptr (dst_pxn);
  if (i < 0) return i;

  /* XXX - verify this wouldn't form a cycle */

  src_pxn->pxn_refcnt++;
  dst_pxn->pxn_bptrs[i].bptr_type = BPTR_PXN;
  dst_pxn->pxn_bptrs[i].bptr_pxn = src_pxn;

  return 0;
}

int sys_pxn_add_pxn (u_int sn, struct Xn_name *dst_xn,
		     struct Xn_name *src_xn, u_int k_dst, u_int k_src) {
  struct Pxn *src_pxn, *dst_pxn;
  struct Xn_name src_xn_kernel, dst_xn_kernel;
  cap c_dst, c_src;
  int r;

  copyin (src_xn, &src_xn_kernel, sizeof (src_xn_kernel));
  copyin (dst_xn, &dst_xn_kernel, sizeof (dst_xn_kernel));

  if (!(src_pxn = lookup_pxn (&src_xn_kernel)))
    return -E_NOT_FOUND;
  if (!(dst_pxn = lookup_pxn (&dst_xn_kernel)))
    return -E_NOT_FOUND;

  if ((r = env_getcap (curenv, k_dst, &c_dst)) < 0) {
    return r;
  }  
  if ((r = env_getcap (curenv, k_src, &c_src)) < 0) {
    return r;
  }  

  return (pxn_add_pxn (dst_pxn, src_pxn, &c_dst, &c_src));
}
  
int pxn_authorizes_xtnt (struct Pxn *pxn, cap *c, struct Xn_xtnt *xtnt, 
			 int access, int *error) {
  int i;

  /* iterate over all the bptrs that point to extents */
  for (i=0; i < pxn->pxn_bptr_sz; i++) {

    /* skip anything not an xtnt */
    if (pxn->pxn_bptrs[i].bptr_type != BPTR_XTNT) continue;

    /* we bail with an error if find a dominating extent, but can't
       get access to it...we don't report an error if we can't find
       an extent yet. */

    if (xtnt_dominates (&pxn->pxn_bptrs[i].bptr_xtnt, xtnt)) {
      if (xtnt_verify_access (pxn, c, access, error)) 
	return 1;
      else
	return 0;
    }
  }

  /* if we didn't find anything in the extents in this pxn, try sub-pxn's
     from this one. */

  /* iterate over all the bptrs that point to pxns */
  for (i=0; i < pxn->pxn_bptr_sz; i++) {

    /* skip anything not a pxn */
    if (pxn->pxn_bptrs[i].bptr_type != BPTR_PXN) continue;
    if (pxn_authorizes_xtnt (pxn->pxn_bptrs[i].bptr_pxn, c, xtnt, access,
			     error)) {
      return 1;
    } else {

      /* bail only if we found a problem other than not finding a dominating
	 extent */

      if (*error != -E_NOT_FOUND) return 0;
    }
  }

  *error = -E_NOT_FOUND;
  return 0;
}

/* XXX - what's this? */
#if 0
  struct Pxn *forged_from;
  struct Xn_xtnt *target;
  struct Xn_xtnt new_left = {0}, new_right = {0};
  int shrunk = 0;

  target = &p->pxn_bptrs[bptr].bptr_xtnt;

  /* XXX very suspect */

  if (xtnt->xtnt_block > target->xtnt_block) {
    /* xtnt starts after target starts */
    new_left.xtnt_block = target->xtnt_block;
    if (xtnt->xtnt_block > target->xtnt_block+target->xtnt_size-1) {
      /* xtnt ends after target ends */
      new_left.xtnt_size = xtnt->xtnt_block+xtnt->xtnt_size - target->xtnt_block;
    } else {
      /* xtnt ends inside target */
      new_left.xtnt_size = xtnt->xtnt_block - target->xtnt_block + 1;
    }
    shrunk = 1;
  } 

  if (xtnt->xtnt_block+xtnt->xtnt_size <= target->xtnt_block+target->xtnt_size) {
    /* xtnt ends before target ends */
    if (xtnt->xtnt_block+xtnt->xtnt_size-1 >= target->xtnt_block) {
      /* xtnt ends in target */
      new_right.xtnt_block = xtnt->xtnt_block+xtnt->xtnt_size-1;
      new_right.xtnt_size = target->xtnt_size - (xtnt->xtnt_block+xtnt->xtnt_size);
      shrunk = 1;
    } else {
      /* xtnt ends before target starts */
      new_right.xtnt_block = target->xtnt_block;
      new_right.xtnt_size = target->xtnt_size;
    }

  }

  /* preserve where these extents came from */
  new_left.xtnt_forged_from = target->xtnt_forged_from;
  new_right.xtnt_forged_from = target->xtnt_forged_from;

  /* we now have four cases:
   * 
   * 1) both new_left and new_right are zero sized meaning that target
   * was completely removed.
   * 2) new_left is non-zero and new_right is zero: replace target with new_left
   * 3) new_right is non-zero and new_left is zero: replace target with new_right
   * 4) both new_right and new_left are non-zero: replace target with new_left
   * and add a new bptr for new_right.
   *
   */
  
  p->pxn_bptrs[bptr].bptr_type = BPTR_EMPTY;
  if (new_left.xtnt_size != 0)
    pxn_add_extent (p, &new_left);
  if (new_right.xtnt_size != 0) 
    pxn_add_extent (p, &new_right);
#endif
