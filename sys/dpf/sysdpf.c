
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

#include <xok_include/assert.h>
#include <xok/mmu.h>
#include <xok/env.h>
#include <xok/sys_proto.h>
#include <dpf/dpf-internal.h>
#include <dpf/old-dpf.h>
#include <xok/bitvector.h>
#include <xok/syscallno.h>
#include <xok/pktring.h>
#include <xok/printf.h>
#include <xok/malloc.h>

#include "../lib/ash/ash_ae_net.h"

#include <xok/kerrno.h>

#define NFILTER DPF_MAXFILT

#define DPF_ACL_LEN 1


/* Glue to hook dpf into xok. Associated with each filter is a list of
 * environments that have references to that filter. A filter is not freed
 * until no envionment references it. */


static u_int refcnt[NFILTER];
#define REF_SZ BV_SZ(NENV)
static u_int refs[NFILTER][REF_SZ];
static cap caps[NFILTER];
static int ringvals[NFILTER];
static u_int final_filters[BV_SZ(NFILTER)];

static void dpf_set_final (u_int filterid);
static void dpf_set_is_not_final (u_int filterid);
static int dpf_is_final (u_int filterid);


/* First kernel-resident cap struct. */
/* Maybe shift to some better allocation system if we need more... */
static cap Dpfcap, *dpfcap = &Dpfcap;
static int dpfcaplen = 1;


/* Add a filter and add a reference to it to env envid. protect the new
   filter with capability k. ke is used to access envid. */

DEF_ALIAS_FN (sys_self_dpf_insert, sys_dpf_insert);
int sys_dpf_insert(u_int sn, u_int kdpf, u_int k, void *up, 
                   int usering, u_int ke, int envid) 
{
  int fid;
  int err = -1;
  struct Env *e;
  struct dpf_ir *p = NULL;
  int sz = sizeof(struct dpf_ir) - ((DPF_MAXELEM + 1) * sizeof (struct ir));
  cap c;
  int r;

  if (sn == SYS_self_dpf_insert)
    e = curenv;
  else if (! (e = env_access (ke, envid, ACL_W, &r)))
    return r;

  if ((err = env_getcap (e, kdpf, &c)) < 0) 
    /* check generic DPF modify cap */
    goto error;

  if ((r = acl_access (&c, dpfcap, dpfcaplen, ACL_W)) < 0)
    return r;

  p = malloc (sizeof(struct dpf_ir));
  if (!p)
    return -E_NO_MEM;

  if ((err = env_getcap (e, k, &c)) < 0) 
    goto error;

  /* This is very structure-layout dependent */
  
  copyin (up, (void *)p, sz);
  if (p->irn >= DPF_MAXELEM) {
    printf ("too many ELEMs in filter description\n");
    err = -E_INVAL;
    goto error;
  }

  copyin (((char *)up + sz), (void *) &p->ir[0], 
          ((p->irn + 1) * sizeof (struct ir)));
  fid = dpf_insert(p);

  if (fid >= NFILTER) {
    printf ("sys_dpf_insert: fid > NFILTER returned from dpf_insert...\n");
    err = -E_INVAL;
    goto error;
  }

  if (fid > 0) {
    caps[fid] = c;
    caps[fid].c_perm = ACL_ALL;
    dpf_check();
    dpf_add_ref (fid, e);
    ringvals[fid] = usering;
    if (usering > 0) {
       pktring_adddpfref (usering, fid);
    }
  } else {
    err = -E_INVAL;
    goto error;
  }

  free (p);
  return fid;

error:
  free (p);
  return err;
}

/* Add a filter and add a reference to it to env envid. protect the new
   filter with capability k. ke is used to access envid. */

DEF_ALIAS_FN (sys_self_dpf_insert_old, sys_dpf_insert_old);
int sys_dpf_insert_old(u_int sn, u_int kdpf, u_int k, void *up, 
                       int s, int usering, u_int ke, int envid) 
{
  int fid;
  int err = -1;
  struct Env *e;
  void *p = NULL;
  int sz = s * sizeof(struct dpf_frozen_ir);
  cap c;
  int r;

  if (sn == SYS_self_dpf_insert_old)
    e = curenv;
  else if (! (e = env_access (ke, envid, ACL_W, &r)))
    return r;

  if ((err = env_getcap (e, kdpf, &c)) < 0) 
    /* check generic DPF modify cap */
    goto error;

  if ((r = acl_access (&c, dpfcap, dpfcaplen, ACL_W)) < 0)
    return r;

  if (sz > NBPG)
    return -1;
  p = malloc (sz);
  if (!p) 
    return -E_NO_MEM;

  if ((err = env_getcap (e, k, &c)) < 0) 
    goto error;

  copyin (up, p, sz);
  fid = dpf_insert(dpf_xlate(p, s));

  if (fid >= NFILTER) {
    printf ("sys_dpf_insert_old: fid > NFILTER returned from dpf_insert...\n");
    err = -E_NO_DPFS;
    goto error;
  }

  if (fid > 0) {
    caps[fid] = c;
    caps[fid].c_perm = ACL_ALL;
    dpf_check();
    dpf_add_ref (fid, e);
    ringvals[fid] = usering;
    if (usering > 0) {
       pktring_adddpfref (usering, fid);
    }
  } else {
    err = -E_INVAL;
    goto error;
  }

  free (p);
  return fid;

error:
  free (p);
  return err;
}


/* free memory used by dpf state atom */
int dpf_free_state(int pid, u_int data);

DEF_ALIAS_FN (sys_self_dpf_free_state, sys_dpf_free_state);
int sys_dpf_free_state(u_int sn, u_int k, u_int fid, 
                       u_int data, u_int ke, int envid) 
{
  struct Env *e;
  cap c;
  int r;

  if (fid < 0 || fid > NFILTER)
    return -1;

  if (sn == SYS_self_dpf_free_state)
    e = curenv;
  else if (! (e = env_access (ke, envid, ACL_W, &r)))
    return r;

  if ((r = env_getcap (e, k, &c)) < 0)
    return r;

  if ((r = acl_access (&c, &caps[fid], DPF_ACL_LEN, ACL_W)) < 0)
    return r;

  dpf_free_state (fid, data);

  return (0);
}


/* remove envid's reference to fid. If no one is referencing the filter, 
   free it. Use cap k to access fid and ke for envid  */

DEF_ALIAS_FN (sys_self_dpf_delete, sys_dpf_delete);
int sys_dpf_delete(u_int sn, u_int k, u_int fid, u_int ke, int envid) 
{
  struct Env *e;
  cap c;
  int r;

  if (fid < 0 || fid > NFILTER)
    return -1;

  if (sn == SYS_self_dpf_delete)
    e = curenv;
  else if (! (e = env_access (ke, envid, ACL_W, &r)))
    return r;

  if ((r = env_getcap (e, k, &c)) < 0)
    return r;

  if ((r = acl_access (&c, &caps[fid], DPF_ACL_LEN, ACL_W)) < 0)
    return r;
 
  dpf_del_ref (fid, e->env_id);

  return (0);
}

/* add a ref to fid to env envid. Use cap k to access fid and ke for envid  */

DEF_ALIAS_FN (sys_self_dpf_ref, sys_dpf_ref);
int sys_dpf_ref (u_int sn, u_int k, u_int fid, u_int ke, int envid) 
{
  struct Env *e;
  cap c;
  int r;

  if (fid < 0 || fid > NFILTER) 
    return -E_INVAL;
  
  if (sn == SYS_self_dpf_ref)
    e = curenv;
  else if (! (e = env_access (ke, envid, ACL_W, &r)))
    return r;

  if ((r = env_getcap (e, k, &c)) < 0)
    return r;

  if ((r = acl_access (&c, &caps[fid], DPF_ACL_LEN, ACL_W)) < 0)
    return r;

  return (dpf_add_ref (fid, e));
}


int sys_dpf_refcnt(u_int sn, u_int k, u_int fid)
{
  struct Env *e = curenv;
  cap c;
  int r;

  if (fid < 0 || fid > NFILTER) 
    return -E_INVAL;
  
  if ((r = env_getcap (e, k, &c)) < 0)
    return r;

  if ((r = acl_access (&c, &caps[fid], DPF_ACL_LEN, ACL_W)) < 0)
    return r;

  return refcnt[fid];
}


/* initialize internal state. must be called before any filters are allocated. */
struct atom dpf_fake_head;      /* Bogus header node. */

void dpf_init_refs () {
  bzero (refs, sizeof (refs));
  bzero (refcnt, sizeof (refcnt));
  bzero (caps, sizeof (caps));
  bzero (ringvals, sizeof (ringvals));
  bzero (final_filters, sizeof (final_filters));
  /* format of a full dpf capability is CAP_NM_DISK.* */
  Dpfcap.c_valid = 1;
  Dpfcap.c_isptr = 0;
  Dpfcap.c_len = 1;
  Dpfcap.c_perm = CL_ALL;
  bzero (Dpfcap.c_name, sizeof (Dpfcap.c_name));
  Dpfcap.c_name[0] = CAP_NM_DPF;
  dpf_base = &dpf_fake_head;
  dpf_iptr = dpf_compile(NULL);
  LIST_INIT(&dpf_base->kids);
  dpf_allocpid();
}


/* return fid associated with the ringval */
int dpf_ringval_getfid (int ringval)
{
  int i;
  for(i=0; i<NFILTER; i++)
  {
    if (ringvals[i] == ringval)
      return i;
  }
  return -1;
}


/* return ringval associated with fid */

int dpf_fid_getringval (int fid)
{
  if (fid < 0 || fid >= NFILTER) {
    printf ("dpf_fid_lookup: got bogus fid\n");
    return (-1);
  }
  return (ringvals[fid]);
}


/* return an envrionment associated with fid */

struct Env *dpf_fid_lookup (int fid) {
  u_int i;

  if (fid <= 0 || fid >= NFILTER) {
    printf ("dpf_fid_lookup: got bogus fid\n");
    return NULL;
  }

  i = bv_ffs (refs[fid], REF_SZ);
  if (i == -1) {
    printf ("dpf_fid_lookup: didn't find env\n");
    return NULL;
  }

  return (&__envs[i]);
}

/* add a reference to a filter from an environment */

int dpf_add_ref (int fid, struct Env *e) {
  if (fid < 0 || fid >= NFILTER)
    return -E_INVAL;

  bv_bs (refs[fid], envidx (e->env_id));
  refcnt[fid]++;
  return (0);
}

/* delete reference to fid from envid */

void dpf_del_ref (u_int fid, int envid) {
  if (!bv_bt (refs[fid], envidx (envid))) {
    printf ("dpf_del_ref: no reference to un-reference for env %d\n",envid);
    return;
  }
  bv_bc (refs[fid], envidx (envid));
  if (!--refcnt[fid]) 
  {
    // printf("dpf_del_ref: refcnt at 0, removing dpf %d\n",fid);
    dpf_delete (fid);
    if (ringvals[fid] > 0) {
       pktring_deldpfref (ringvals[fid], fid);
       ringvals[fid] = 0;
    }
  }
}

/* remove all references to filters from an environment */

void dpf_del_env (int envid) {
  int i;
  
  for (i = 0; i < NFILTER; i++) {
    if (bv_bt (refs[i], envidx (envid)))
	dpf_del_ref (i, envid);
  }
}


/* check for modify access to a DPF filter */

int dpf_check_modify_access (struct Env *e, u_int k, u_int filterid)
{
   cap c;
   int r;

   if ((filterid < 0) || (filterid >= DPF_MAXFILT)) {
      printf ("access check failure 1\n");
      return (-1);
   }

   if ((r = env_getcap (e, k, &c)) < 0) 
   {
      printf ("access check failure 2 (k %d)\n", k);
      return r;
   }

   if ((r = acl_access (&c, &caps[filterid], DPF_ACL_LEN, ACL_W)) < 0) {
     printf ("access check failure 3\n");
     return r;
   }

   return (0);
}

/* XXX -- the final check is not yet used. */

/* mark a filter as final */
static void dpf_set_final (u_int filterid) {
  assert (filterid < NFILTER);
  bv_bs (final_filters, filterid);
}

/* mark a filter as not final */
static void dpf_set_is_not_final (u_int filterid) {
  assert (filterid < NFILTER);
  bv_bc (final_filters, filterid);
}

/* check if a filter is final or not */
static int dpf_is_final (u_int filterid) {
  assert (filterid < NFILTER);
  return (bv_bt (final_filters, filterid));
}

/* route filter's matches to a new pktring */

DEF_ALIAS_FN (sys_self_dpf_pktring, sys_dpf_pktring);
int sys_dpf_pktring (u_int sn, u_int k, u_int fid, 
                     int ringid, u_int ke, int envid) 
{
  struct Env *e;
  int oldringid;
  cap c;
  int r;

  if (fid < 0 || fid > NFILTER)
    return -1;

  if (sn == SYS_self_dpf_pktring)
    e = curenv;
  else if (! (e = env_access (ke, envid, ACL_W, &r)))
    return r;

  if ((r = env_getcap (e, k, &c)) < 0)
    return r;

  if ((r = acl_access (&c, &caps[fid], DPF_ACL_LEN, ACL_W)) < 0)
    return r;

  oldringid = ringvals[fid];
  ringvals[fid] = ringid;
  if (ringid > 0) {
     pktring_adddpfref (ringid, fid);
  }
  if (oldringid > 0) {
     pktring_deldpfref (oldringid, fid);
  }

  return (0);
}


void sys_dpf_debug(u_int sn)
{
  dpf_output();
}
