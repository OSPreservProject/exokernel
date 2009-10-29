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

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <xok/sys_ucall.h>
#include <xok/wk.h>
#include <xok/sysinfo.h>
#include <xok/env.h>

#include <vos/errno.h>
#include <vos/proc.h>
#include <vos/vm.h>
#include <vos/cap.h>
#include <vos/wk.h>
#include <vos/assert.h>


#define dprintf if(0) kprintf


/* default tags for reserved terms - should not be used by user programs */
#define UWK_RESV_TAG_MIN 0x30000000
#define UWK_RESV_TAG_MAX 0x3000FFFF

/* 
 * space for constructing the super-predicate (i.e. one that contains both the
 * specified conditions and the pre-registered extras).
 */

#define DEFAULT_MALLOCED_WKLEN	16
static int superlen = DEFAULT_MALLOCED_WKLEN;
static struct wk_term static_wkarray[DEFAULT_MALLOCED_WKLEN];
static struct wk_term *wksuper = static_wkarray;

/* information about the registered predicates */

typedef struct 
{
  int (*construct) (struct wk_term * t, u_quad_t param);
  void (*callback) (u_quad_t param);
  u_quad_t param;
  int maxlen;
} wk_extra_t;

#define MAX_EXTRAS	16
static wk_extra_t wk_extras[MAX_EXTRAS];
static int wk_extra_maxlen = 0; /* starts with 0, incr by wk_register_extra */



/* wk_waitfor_pred_directed: constructs a larger predicate (including both the
 * specified wk_terms and the registered terms), and then yields the time
 * slice to the specified envid.  If one of the registered terms causes the
 * predicate to evaluate to TRUE, then the associated call-back will be
 * invoked and the wait/yield repeats.
 */
int 
wk_waitfor_pred_directed (struct wk_term *t, u_int sz, int envid)
{
  extern void flush_io();
  int ret;
  int origsz = sz;
  int needsz = sz + wk_extra_maxlen;
  int i;

  if (sz == 0) 
    RETERR(V_WK_BADSZ);

  /* allocate new array if previous array not long enough */
  if (needsz > superlen)
  {
    /* last wk term array was dynamically allocated, so free first */
    if (wksuper != static_wkarray) 
      free (wksuper);

    wksuper = malloc (needsz * sizeof (struct wk_term));
      
    if (wksuper == NULL)
      RETERR(V_NOMEM);

    superlen = needsz;
  }

  memmove (wksuper, t, (sz * sizeof (struct wk_term)));

wait_again:
  for (i = 0; i < MAX_EXTRAS; i++)
  {
    if (wk_extras[i].construct)
    {
      int tmpsz = sz;
      tmpsz = wk_mkop (tmpsz, wksuper, WK_OR);
      tmpsz = wk_mktag (tmpsz, wksuper, (UWK_RESV_TAG_MIN + i));
      ret = wk_extras[i].construct (&wksuper[tmpsz], wk_extras[i].param);
	  
      if (ret > wk_extras[i].maxlen)
      {
        dprintf ("wk_waitfor_pred: extra predicate exceeds pre-specified"
    	         " maxlen (%d > %d)\n", ret, wk_extras[i].maxlen);
        dprintf ("more info: construct %p, callback %p, param %qu\n",
    	         wk_extras[i].construct, wk_extras[i].callback,
    	         wk_extras[i].param);
        RETERR(V_WK_BADREG);
      }
      else if (ret > 0)
        sz = tmpsz + ret;
    }
  }

  /* if you add on more terms here, make sure you updated the static initial 
   * value of wk_extra_maxlen above too */
  
 
  /* 
   * flush io before we go wait, so we don't hold anything pending
   */
  flush_io();

  /* download the predicate to kernel */
  if ((ret = sys_wkpred (wksuper, sz)) < 0)
  {
    printf ("wk_waitfor_pred_directed: unable to download wkpred"
  	    " (ret %d, sz %d)\n", ret, sz);
    RETERR(V_WK_DLFAILED);
  }

  /* put self to sleep */
  UAREA.u_pred_tag = 0;
  if (UAREA.u_status > 0) UAREA.u_status --;
  env_yield (envid);

  /* woke up, let's figure out why */

  if ((UAREA.u_pred_tag >= UWK_RESV_TAG_MIN) 
      && (UAREA.u_pred_tag <= UWK_RESV_TAG_MAX))
    /* registered terms triggered the wakeup */
  {
    i = UAREA.u_pred_tag - UWK_RESV_TAG_MIN;
    
    if (wk_extras[i].construct && wk_extras[i].callback)
      wk_extras[i].callback (wk_extras[i].param);
  
    /* now go back to wait for the real predicate to expire */
    sz = origsz;
    envid = -1;
    goto wait_again;
  }

  /* this is just to get rid of the predicate, so it won't accidentally */
  /* get used in a subsequent non-predicated yield.                     */
  if ((ret = sys_wkpred (NULL, 0)) < 0)
  {
    dprintf ("wk_waitfor_pred_directed: unable to eliminate wkpred"
	     " (ret %d)\n", ret);
    RETERR(V_WK_DLFAILED);
  }

  return 0;
}


/* wk_register_extra: registers extra predicate terms */
int 
wk_register_extra (int (*construct) (struct wk_term *, u_quad_t),
		   void (*callback) (u_quad_t), u_quad_t param, u_int maxlen)
{
  int i;

  if (maxlen == 0)
    RETERR(V_WK_BADSZ);

  if (construct == NULL)
    RETERR(V_WK_BADCONS);

  for (i = 0; i < MAX_EXTRAS; i++)
  {
    if (wk_extras[i].construct == NULL)
    {
      wk_extras[i].construct = construct;
      wk_extras[i].callback = callback;
      wk_extras[i].param = param;
      wk_extras[i].maxlen = maxlen;
      wk_extra_maxlen += maxlen + 2;	/* pred plus WK_OR */
      return i;
    }
  }
  
  dprintf ("wk_register_extra: failed due to lack of space in wk_extras\n");
  RETERR(V_WK_MTERMS);
}



/* wk_deregister_extra: unregister a term */
int 
wk_deregister_extra (int (*construct) (struct wk_term *, u_quad_t),
		     u_quad_t param)
{
  int i;

  for (i = 0; i < MAX_EXTRAS; i++)
  {
    if ((wk_extras[i].construct == construct) &&
        (wk_extras[i].param == param))
    {
      wk_extras[i].construct = NULL;
      wk_extra_maxlen -= wk_extras[i].maxlen + 2;
      return 0;
    }
  }
  dprintf ("wk_deregister_extra: extra not found (construct %p, param %qu)\n",
 	   construct, param);
  RETERR(V_WK_TNOTFOUND);
}



/* misc user-level functions for dealing with the wk predicates */
void 
wk_waitfor_value (int *addr, int value, u_int cap)
{
  struct wk_term t[UWK_MKCMP_EQ_PRED_SIZE];
  int sz = 0;

  sz = wk_mkcmp_eq_pred (&t[0], addr, value, cap);
  assert (sz == UWK_MKCMP_EQ_PRED_SIZE);

  while (*addr != value)
  {
    wk_waitfor_pred (t, sz);
  }
}


void 
wk_waitfor_value_neq (int *addr, int value, u_int cap)
{
  struct wk_term t[UWK_MKCMP_NEQ_PRED_SIZE];
  int sz = 0;

  sz = wk_mkcmp_neq_pred (&t[0], addr, value, cap);
  assert (sz == UWK_MKCMP_NEQ_PRED_SIZE);

  while (*addr == value)
  {
    wk_waitfor_pred (t, sz);
  }
}


void 
wk_waitfor_value_lt (int *addr, int value, u_int cap)
{
  struct wk_term t[3];
  int sz = 0;

  sz = wk_mkvar (sz, t, wk_phys (addr), cap);
  sz = wk_mkimm (sz, t, value);
  sz = wk_mkop (sz, t, WK_LT);

  while (*addr >= value)
  {
    wk_waitfor_pred (t, sz);
  }
}


void 
wk_waitfor_value_gt (int *addr, int value, u_int cap)
{
  struct wk_term t[3];
  int sz = 0;

  sz = wk_mkvar (sz, t, wk_phys (addr), cap);
  sz = wk_mkimm (sz, t, value);
  sz = wk_mkop (sz, t, WK_GT);

  while (*addr <= value)
  {
    wk_waitfor_pred (t, sz);
  }
}


/* construct a predicate term that is true when *addr == value */
int 
wk_mkcmp_eq_pred (struct wk_term *t, int *addr, int value, u_int cap)
{
  int sz = 0;
  sz = wk_mkvar (sz, t, wk_phys (addr), cap);
  sz = wk_mkimm (sz, t, value);
  sz = wk_mkop (sz, t, WK_EQ);
  return sz;
}


/* construct a predicate term that is true when *addr != value */
int 
wk_mkcmp_neq_pred (struct wk_term *t, int *addr, int value, u_int cap)
{
  int sz = 0;
  sz = wk_mkvar (sz, t, wk_phys (addr), cap);
  sz = wk_mkimm (sz, t, value);
  sz = wk_mkop (sz, t, WK_NEQ);
  return sz;
}


/* contstruct a predicate term that is true when *addr > value */
int 
wk_mkcmp_gt_pred (struct wk_term *t, int *addr, int value, u_int cap)
{
  int sz = 0;

  sz = wk_mkvar (sz, t, wk_phys (addr), cap);
  sz = wk_mkimm (sz, t, value);
  sz = wk_mkop (sz, t, WK_GT);
  return sz;
}


/* construct a predicate term that always evaluates to false */
int 
wk_mkfalse_pred (struct wk_term *t)
{
  int sz = 0;
  sz = wk_mkimm (sz, t, 0);
  sz = wk_mkimm (sz, t, 0);
  sz = wk_mkop (sz, t, WK_NEQ);
  return sz;
}


/* construct a predicate term that always evaluates to true */
int 
wk_mktrue_pred (struct wk_term *t)
{
  int sz = 0;
  sz = wk_mkimm (sz, t, 0);
  sz = wk_mkimm (sz, t, 0);
  sz = wk_mkop (sz, t, WK_EQ);
  return sz;
}


/* has to split up ticks into longs for the wk_pred code */
int 
wk_mksleep_pred (struct wk_term *t, u_quad_t ticks)
{
  int sz = 0;

  /* if low bits greater */
  sz = wk_mkvar (sz, t, wk_phys (&__sysinfo.si_system_ticks), CAP_USER);
  sz = wk_mkimm (sz, t, (u32) ticks);
  sz = wk_mkop (sz, t, WK_GT);

  /* and high bits the same */
  sz = wk_mkvar 
    (sz, t, wk_phys (&((u32 *) & __sysinfo.si_system_ticks)[1]), CAP_USER);
  sz = wk_mkimm (sz, t, ((u32 *) & ticks)[1]);
  sz = wk_mkop (sz, t, WK_EQ);
  
  /* or high bits greater */
  sz = wk_mkop (sz, t, WK_OR);
  sz = wk_mkvar 
    (sz, t, wk_phys (&((u32 *) & __sysinfo.si_system_ticks)[1]), CAP_USER);
  sz = wk_mkimm (sz, t, ((u32 *) & ticks)[1]);
  sz = wk_mkop (sz, t, WK_GT);

  assert (sz <= UWK_MKSLEEP_PRED_SIZE);
  return sz;
}

int 
wk_mkusleep_pred (struct wk_term *t, u_int usecs)
{
  int sz = 0;
  unsigned long long ticks = 
    ((usecs+__sysinfo.si_rate-1)/__sysinfo.si_rate)+__sysinfo.si_system_ticks;
  sz = wk_mksleep_pred (t, ticks);
  return (sz);
}




/* has to split up value into longs for the wk_pred code */
int 
wk_mkenvdeath_pred (struct wk_term *t)
{
  int sz = 0;
  u_quad_t count = __sysinfo.si_killed_envs;

  /* if low bits greater */
  sz = wk_mkvar (sz, t, wk_phys (&__sysinfo.si_killed_envs), CAP_USER);
  sz = wk_mkimm (sz, t, (u32) count);
  sz = wk_mkop (sz, t, WK_GT);
  
  /* and high bits the same */
  sz = wk_mkvar 
    (sz, t, wk_phys (&((u32 *) & __sysinfo.si_killed_envs)[1]), CAP_USER);
  sz = wk_mkimm (sz, t, ((u32 *) & count)[1]);
  sz = wk_mkop (sz, t, WK_EQ);
 
  /* or high bits greater */
  sz = wk_mkop (sz, t, WK_OR);
  sz = wk_mkvar 
    (sz, t, wk_phys (&((u32 *) & __sysinfo.si_killed_envs)[1]), CAP_USER);
  sz = wk_mkimm (sz, t, ((u32 *) & count)[1]);
  sz = wk_mkop (sz, t, WK_GT);

  assert (sz <= UWK_MKENVDEATH_PRED_SIZE);
  return sz;
}



int 
wk_waitfor_freepages ()
{
  struct wk_term t[10];
  int sz = 0;

  /* Wait for
   * 
   * (1) the number of free pages to increase, or
   * (2) the number of dirty pages to decrease, or 
   * (3) the number of pinned pages to decrease. 
   * 
   * Any of these can have the effect of making more allocatable pages.
   */

  if (__sysinfo.si_nfreepages <= 
      (__sysinfo.si_ndpages + __sysinfo.si_pinnedpages))
  {
    u_int ndpages = __sysinfo.si_ndpages;
    u_int freepages = __sysinfo.si_nfreepages;
    u_int pinnedpages = __sysinfo.si_pinnedpages;
    sz = wk_mkvar (sz, t, wk_phys (&__sysinfo.si_nfreepages), CAP_USER);
    sz = wk_mkimm (sz, t, freepages);
    sz = wk_mkop (sz, t, WK_GT);
    sz = wk_mkop (sz, t, WK_OR);
    sz = wk_mkvar (sz, t, wk_phys (&__sysinfo.si_ndpages), CAP_USER);
    sz = wk_mkimm (sz, t, ndpages);
    sz = wk_mkop (sz, t, WK_LT);
    sz = wk_mkop (sz, t, WK_OR);
    sz = wk_mkvar (sz, t, wk_phys (&__sysinfo.si_pinnedpages), CAP_USER);
    sz = wk_mkimm (sz, t, pinnedpages);
    sz = wk_mkop (sz, t, WK_LT);
    return wk_waitfor_pred (t, sz);
  }

  RETERR(V_NOMEM);
}


void 
wk_dump (struct wk_term *t, int sz)
{
  if (!sz)
    return;

  switch (t[0].wk_type)
    {
    case WK_VAR:
      printf ("WK_VAR %x %d\n", (u_int) t[0].wk_var, t[0].wk_cap);
      break;
    case WK_IMM:
      printf ("WK_IMM %d\n", t[0].wk_imm);
      break;
    case WK_TAG:
      printf ("WK_TAG %d\n", t[0].wk_tag);
      break;
    case WK_OP:
      switch (t[0].wk_op)
	{
	case WK_GT:
	  printf ("WK_GT\n");
	  break;
	case WK_GTE:
	  printf ("WK_GTE\n");
	  break;
	case WK_LT:
	  printf ("WK_LT\n");
	  break;
	case WK_LTE:
	  printf ("WK_LTE\n");
	  break;
	case WK_EQ:
	  printf ("WK_EQ\n");
	  break;
	case WK_NEQ:
	  printf ("WK_NEQ\n");
	  break;
	case WK_OR:
	  printf ("WK_OR\n");
	  break;
	default:
	  printf ("WK_OP <invalid>");
	  break;
	}
      break;
    default:
      printf ("<invalid (%d)>\n", t[0].wk_type);
    }

  wk_dump (&t[1], sz - 1);
}


/* wk_waitfor_timeout: wait for a term, but timeout after certain number of
 * ticks */
int 
wk_waitfor_timeout (struct wk_term *u, int sz0, int ticks, int envid)
{
  struct wk_term t[128];
  int sz = sz0, i;
  int ret;
  unsigned long long dest_ticks = ticks;

#define DID_PRED    666
#define DID_TIMEOUT 777

  if (sz0 > 0)
  {
    i = wk_mktag (0, t, DID_PRED);
    memmove (t + i, u, (sz * sizeof (struct wk_term)));
    sz += i;
  }

  /* for timeout */
  if (dest_ticks > 0)
  {
    if (sz0 > 0)
      sz = wk_mkop (sz, t, WK_OR);
    sz = wk_mktag (sz, t, DID_TIMEOUT);
    dest_ticks += __sysinfo.si_system_ticks;
    sz += wk_mksleep_pred (&t[sz], dest_ticks);
  }

  UAREA.u_pred_tag = 0;
  if ((ret = wk_waitfor_pred_directed (t, sz, envid)) < 0)
    return ret;

  switch (UAREA.u_pred_tag)
  {
    case DID_TIMEOUT:
      RETERR (V_WOULDBLOCK);
    case DID_PRED:
      return 0;
  }

  RETERR (V_WAKENUP);
}


int
wk_sleep(u_int secs, int envid)
{
  u_int ticks = SECONDS2TICKS(secs);
  return wk_waitfor_timeout(0L, 0, ticks, envid);
}


int 
wk_sleep_usecs (u_int usecs, int envid)
{
  u_int ticks = USECONDS2TICKS(usecs);
  return wk_waitfor_timeout(0L, 0, ticks, envid);
}


int
nanosleep(const struct timespec *req, struct timespec *rem)
{
  u_int usecs;
  u_int r;
  u_int to;
  u_int still;

  if (req == 0)
    RETERR(EINVAL);
  usecs = req->tv_sec * 1000000;
  usecs += (req->tv_nsec / 1000);
  
  to = __sysinfo.si_system_ticks + USECONDS2TICKS(usecs);
  r = wk_sleep_usecs(usecs, -1);

  if (r == -1 && errno == V_WOULDBLOCK)
    return 0;
  else
  {
    still = to - __sysinfo.si_system_ticks;
    if (rem != 0 && still > 0)
    {
      usecs = TICKS2USECONDS(still);
      if (usecs > 1000000)
      {
	rem->tv_sec = (int) (usecs/1000000);
	usecs -= rem->tv_sec*1000000;
	if (usecs < 0) usecs = 0;
      }
      rem->tv_nsec = usecs * 1000;
    }
    RETERR(EINTR);
  }
  return 0;
}


