
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


#include <xok/env.h>
#include <xok/kerrno.h>
#include <xok/sys_proto.h>
#include <xok/syscallno.h>
#include <xok/malloc.h>
#include <xok/printf.h>
#include <xok_include/assert.h>

inline int
acl_access (cap *c, cap *acl, u_short acl_len, const u_char perm) 
  __XOK_REQ_SYNC(on acllist)
{
  if (!c->c_valid) return -E_CAP_INVALID;

  if ((perm & c->c_perm) != perm) return -E_CAP_INSUFF;

  for (; acl_len--; acl++)
    if (acl->c_valid) {
      if (acl->c_isptr && !acl_access (c, acl->cl_list, acl->cl_len, perm))
	return 0;
      if ((perm & acl->c_perm) == perm && c->c_len <= acl->c_len
	  && !memcmp (c->c_name, acl->c_name, c->c_len))
	return 0;
    }

  return -E_CAP_INSUFF;
}

int
acl_read (cap *acl, u_short acl_len, cap *res, u_short res_len)
{
  cap * const le = &acl[acl_len-1];
  const int p = (le->c_valid && le->c_isptr);
  int m = page_fault_mode;
  int n1, n2;

  page_fault_mode = PFM_PROP;
  res = trup (res);

  n1 = min (res_len, acl_len - p);
  novbcopy (acl, res, n1 * sizeof (cap));
  if (res_len <= n1 || !p) {
    page_fault_mode = m;
    return (n1);
  }

  n2 = min (res_len - n1, le->cl_len);
  novbcopy (le->cl_list, res + n1, n2 * sizeof (cap));
  page_fault_mode = m;
  return (n1 + n2);
}

int
acl_setsize (cap *acl, u_short acl_len, u_short size)
{
  cap * const le = &acl[acl_len-1];
  const int p = (le->c_valid && le->c_isptr);
  cap *eacl;

  if (size <= acl_len) {
    if (p) {
      eacl = le->cl_list;
      *le = *le->cl_list;
      free (eacl);
    }
  }
  else {
    const int eacl_len = size - acl_len;

    if (eacl_len > NBPG / sizeof (cap))
      return -E_ACL_SIZE;

    eacl = malloc (eacl_len * sizeof (cap));
    if (!eacl) return -E_NO_MEM;
    bzero (eacl, eacl_len * sizeof (cap));

    if (p)
      bcopy (le->cl_list, eacl, min (le->cl_len, eacl_len) * sizeof (cap));
    else
      eacl[0] = *le;

    le->c_valid = le->c_isptr = 1;
    le->cl_len = eacl_len;
    le->cl_list = eacl;
  }

  return (0);
}

static inline cap *
acl_getpos (cap *acl, u_short acl_len, u_int pos, int *error)
{
  cap * const le = &acl[acl_len-1];
  const int p = (le->c_valid && le->c_isptr);

  if (pos < acl_len - p)
    return (acl + pos);
  else if (! p) {
    *error = -E_CAP_INVALID;
    return (NULL);
  }
  else
    return (acl_getpos (le->cl_list, le->cl_len, pos - acl_len, error));
}

int
acl_mod (cap *c, cap *acl, u_short acl_len, u_short pos, cap *n)
{
  cap *mc;
  int m = page_fault_mode;
  int r;

  /* Does the requestor have permissions to modify this ACL? */
  if ((r = acl_access (c, acl, acl_len, ACL_MOD)) < 0)
    return r;
  /* Does the pos argument make sense? */
  if (! (mc = acl_getpos (acl, acl_len, pos, &r)))
    return r;

  page_fault_mode = PFM_PROP;
  n = trup (n);

  /* Entry 0 must always have all permissions (so someone can control
   * the resource). */
  if (pos == 0 && !(n->c_valid && n->c_perm == ACL_ALL)) {
    page_fault_mode = m;
    return -E_ACL_INVALID;
  }

  if (n && n->c_valid) {
    /* No pointers allowed */
    if (n->c_isptr) {
      page_fault_mode = m;
      return -E_ACL_INVALID;
    }
    *mc = *n;
  }
  else
    bzero (mc, sizeof (*mc));

  page_fault_mode = m;
  return (0);
}



void
sys_capdump (u_int sn)
{
  int i, j;

  MP_RWLOCK_READ_GET(&(curenv->cap_rwlock));
  printf ("Capability dump for env 0x%x\n", (u_int) curenv->env_id);
  for (i = 0; i < curenv->env_clen; i++)
    if (curenv->env_clist[i].c_valid) {
      printf ("  * %d `", i);
      for (j = 0; j < curenv->env_clist[i].c_len; j++)
	printf ("%s%d", j ? "." : "", curenv->env_clist[i].c_name[j]);
      printf ("' [%x]\n", curenv->env_clist[i].c_perm);
    }
  MP_RWLOCK_READ_RELEASE(&(curenv->cap_rwlock));
}

/* sys_forge: forges a new capability <new> (which must be dominated
 * by existing capability #<k>) and stores it at position #<kd> in
 * environment <envid> (which environment must be writeable via
 * capability #<ke>).
 *
 * sys_self_forge: same thing only ke and envid are omitted and the
 * destination environment is <curenv>.
 */
DEF_ALIAS_FN (sys_self_forge, sys_forge);
int
sys_forge (u_int sn, u_int k, u_int kd, cap *new, u_int ke, int envid)
  __XOK_SYNC(locks e->cap_rwlock)
{
  cap c;
  struct Env *e;
  int m = page_fault_mode;
  int error;

  if (sn == SYS_self_forge)
    e = curenv;
  else if (! (e = env_access (ke, envid, ACL_W, &error)))
    return error;

  /* Capability indices must be in range.  The capability at index 0
   * is the capability with access to the environment, and so can only
   * be overwritten with a complete (perm == ACL_ALL) capability.  
   */
  if (kd >= e->env_clen)
    return (-E_INVAL);
  if ((error = env_getcap (curenv, k, &c)) < 0)
    return error;

  page_fault_mode = PFM_PROP;
  new = trup (new);

  if (kd == 0 && !(new && new->c_valid && new->c_perm == ACL_ALL)) {
    page_fault_mode = m;
    return (-E_CAP_INSUFF);
  }

  /* If the new capability is invalid, treat this as a request to
   * destroy the destination capibility.
   */
  if (! new || ! new->c_valid) 
  {
    MP_RWLOCK_WRITE_GET(&e->cap_rwlock);
    bzero (&e->env_clist[kd], sizeof (cap));
    MP_RWLOCK_WRITE_RELEASE(&e->cap_rwlock);
    
    page_fault_mode = m;
    return (0);
  }

  /* Make sure we can dup and that s dominates the new capability */
  if (! (c.c_perm & CL_DUP) || ! cap_dominates (&c, new))
    return (-E_CAP_INSUFF);

  MP_RWLOCK_WRITE_GET(&e->cap_rwlock);
  e->env_clist[kd] = *new;
  MP_RWLOCK_WRITE_RELEASE(&e->cap_rwlock);

  page_fault_mode = m;
  return (kd);
}

/* sys_acquire: acquire a capability that has been granted via the
 * Uenv structure.  k is the position in which to insert the new
 * capability.
 */
int
sys_self_acquire (u_int sn, u_int k, u_int genvid)
  __XOK_SYNC(locks e->cap_rwlock)
{
  struct Env *e;
  cap c;
  int error;

  /* k must be a valid slot */
  if (k == 0 || k >= curenv->env_clen)
    return -E_INVAL;
  
  /* there must be a valid granting environment with a resident Uenv */
  if (! (e = env_id2env (genvid, &error))) return error;
  if (! e->env_u) return -E_BAD_ENV;

  if (e->env_u->u_aenvid != curenv->env_id)
    return -E_BAD_ENV;
 
  /* technically we should sync access to e->env_u, but since only one env can
   * get a granted cap, we assume there will be no collission between curenv
   * and the giver */

  /* the granted cap must be valid, duplicable and owned by grantor */
  if ((error = env_getcap (e, e->env_u->u_gkey, &c)) < 0) return error;

  if (!cap_dominates (&c, &e->env_u->u_gcap) || !(c.c_perm & CL_DUP))
    return -E_CAP_INSUFF;

  e->env_u->u_aenvid = 0;
  
  MP_RWLOCK_WRITE_GET(&curenv->cap_rwlock);
  curenv->env_clist[k] = e->env_u->u_gcap;
  MP_RWLOCK_WRITE_RELEASE(&curenv->cap_rwlock);
  
  return (0);
}


