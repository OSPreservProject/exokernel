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

#ifndef __LOCKS_H__
#define __LOCKS_H__

#include <stdio.h>
#include <xok/env.h>
#include <xok/wk.h>
#include <xok/sys_ucall.h>
#include <exos/uwk.h>
#include <exos/conf.h>
#include <exos/osdecl.h>
#include <exos/process.h>


struct exos_lock {      /* 6 bytes */
  volatile uint32 lock;	/* 4 byte  - the lock itself: 0 if unused, 
			 *                            envid otherwise */
  uint16 depth;		/* 2 bytes - recursive locking depth */
};

typedef struct exos_lock exos_lock_t;

extern exos_lock_t *global_lock;


/* lock debugging message level:
 *   0: off
 *   1: low
 *   2: high
 *  >9: by module
 */

#define __LOCKS_DEBUG_LVL 0
#define __LOCKS_DEBUG_LOW 30

#define __PROCD_LD 	10
#define __IPC_LD 	40
#define __PFAULT_LD 	50
#define __UAREA_LD 	60
#define __PPAGE_LD 	70
#define __BC_LD 	80

#ifdef __LOCKS_DEBUG__

#define EXOS_LOCKS_SHM_SZ  sizeof(exos_lock_t)+NENV*sizeof(u_int)

#define dlockputs(l,s) \
  if (__LOCKS_DEBUG_LVL > 9 && l == __LOCKS_DEBUG_LVL) sys_cputs(s); \
  else if (__LOCKS_DEBUG_LVL == 1 && l < __LOCKS_DEBUG_LOW) sys_cputs(s); \
  else if (__LOCKS_DEBUG_LVL == 2) sys_cputs(s);

extern u_int* locks_debug_table;

static inline void
exos_locks_debug_init()
{
  int i=0;
  locks_debug_table = (u_int*) ((u_long)global_lock)+sizeof(exos_lock_t);

  for(i=0;i<NENV;i++)
  {
    locks_debug_table[i] = 0;
  }
}

#else /* ! __LOCKS_DEBUG__ */

#define EXOS_LOCKS_SHM_SZ  sizeof(exos_lock_t)
#define dlockputs(l,s)

#endif /* ! __LOCKS_DEBUG__ */



/* test_and_set: atomicly writes new to *dst if *dst == val. 
 * either way, returns the orignal value of dst 
 */
static inline unsigned int
prim_test_and_set (volatile unsigned int *dst, unsigned int val, unsigned int new)
{
  unsigned int result;
  asm ("lock\n"
       "\t cmpxchgl   %2, %0\n"
       : "=m" (*dst), "=a" (result)
       : "r" (new), "m" (*dst), "a" (val)
       : "eax", "cc", "memory");
  return (result);
}


static inline void
exos_lock_init (exos_lock_t *the_lock)
{
  the_lock->lock = 0;
  the_lock->depth = 0; 
}


#ifdef __SMP__

/* spin waits for a lock, return only if the lock is acquired */
static inline void
exos_lock_get_spin (exos_lock_t *the_lock)
{
  uint32 orig_eid;
 
  if (the_lock->lock == __envid)
  {
    the_lock->depth++;
    return;
  }
  
test_n_set:
  orig_eid = prim_test_and_set ((unsigned int *)the_lock, 0, __envid);

  if (orig_eid == 0)
  {
#ifdef __LOCKS_DEBUG__
    locks_debug_table[envidx(__envid)] = 0;
#endif
    /* got the lock */
    the_lock->depth++;
    return;
  }

  else /* didn't get the lock */
  {
#ifdef __LOCKS_DEBUG__
    locks_debug_table[envidx(__envid)] = orig_eid;
#endif
    while(the_lock->lock != 0)
    {
      asm volatile ("" : : : "memory");  
      /* this is to tell gcc not to regalloc the lock */
    }
    goto test_n_set;
  }
}

#endif

/* attempts to obtain a lock: return 0 if successful, eid of the
 * lock owner if not
 */
static inline int
exos_lock_try (exos_lock_t *the_lock)
{
  uint32 orig_eid;

  if (the_lock->lock == __envid)
  {
    the_lock->depth++;
    return 0;
  }

  orig_eid = prim_test_and_set ((unsigned int *)the_lock, 0, __envid);

  if (orig_eid == 0)
  {
    /* got the lock */
    the_lock->depth++;
    return 0;
  }

  else /* didn't get the lock */
  {
    return orig_eid;
  }
}


static inline void 
exos_lock_get_nb (exos_lock_t *the_lock)
{
  int lock_owner;

  while (1) {
    
    lock_owner = exos_lock_try (the_lock);
   
    if (lock_owner != 0) 
    {
      if (__envs[envidx(lock_owner)].env_status != ENV_OK ||
	  __envs[envidx(lock_owner)].env_id != lock_owner) 
      {
	kprintf("EXOS: grab lock from dead env (%d)\n",lock_owner);
	/* if the holder is dead, then forcibly grab lock */
	if (lock_owner == 
	    prim_test_and_set((unsigned int *)the_lock, lock_owner, __envid))
	{
          the_lock->depth = 1;
	  break; /* we now have the lock */
	}
	else
	{
	  continue; /* someone else got it in the meantime so start over */
	}
      } 
      
      else 
      {
	/* wake up when lock released or holder is dead */
	struct wk_term t[3 * (UWK_MKCMP_NEQ_PRED_SIZE + 1)];
	int wk_sz;
	
	/* XXX - can't sleep until lock == 0 because we need to make
	   sure we're waiting for proper env to die as well. Unfortunately,
	   this could lead to bad performance for a heavily contested
	   short-lived lock such that everyone is recreating their
	   wake-up preds quite often, but at least it's correct. */

	wk_sz = wk_mkcmp_neq_pred(&t[0], (void*)the_lock, lock_owner, 0);
	wk_sz = wk_mkop(wk_sz, t, WK_OR);
	wk_sz += wk_mkcmp_neq_pred(&t[wk_sz],
				   &__envs[envidx(lock_owner)].env_status,
				   ENV_OK, 0);
	wk_sz = wk_mkop (wk_sz, t, WK_OR);
	wk_sz += wk_mkcmp_neq_pred(&t[wk_sz],
				   &__envs[envidx(lock_owner)].env_id,
				   lock_owner, 0);

#ifdef __LOCKS_DEBUG__
        locks_debug_table[envidx(__envid)] = lock_owner;
#endif
	wk_waitfor_pred_directed(t, wk_sz, lock_owner);
#ifdef __LOCKS_DEBUG__
        locks_debug_table[envidx(__envid)] = 0;
#endif
      }
    
    } 

    else 
    {
      break; /* we have the lock */
    }
  }
}

static inline int
exos_lock_release (exos_lock_t *the_lock)
{
  if (the_lock->lock == 0) 
  {
    kprintf("EXOS: (env %d) trying to release an empty lock\n", __envid);
    assert(the_lock->lock != 0);
    return -1;
  }
  
  assert(the_lock->depth > 0);

  if (the_lock->lock != __envid) 
  {
    kprintf("EXOS: (env %d) trying to release another env's lock (env %d)\n",
	   __envid, the_lock->lock);
    assert(the_lock->lock == __envid);
    return -1;
  }

  the_lock->depth--;

  if (the_lock->depth == 0) {
    exos_lock_init(the_lock);
  }
  return 0;
}

extern void ExosLocksInit();        /* initializes shared lock segments */


#ifdef __SMP__

#if 0
#define EXOS_LOCK(ln) \
    if (global_lock != (exos_lock_t*)0) exos_lock_get_nb(global_lock); 

#define EXOS_SPINLOCK(ln) \
    if (global_lock != (exos_lock_t*)0) exos_lock_get_spin(global_lock); 

#define EXOS_NBLOCK(ln)	\
    if (global_lock != (exos_lock_t*)0) exos_lock_get_nb(global_lock); 	

#define EXOS_UNLOCK(ln)	\
    if (global_lock != (exos_lock_t*)0) exos_lock_release(global_lock); 
#endif

#define EXOS_LOCK(ln)
#define EXOS_SPINLOCK(ln)
#define EXOS_NBLOCK(ln)	
#define EXOS_UNLOCK(ln) 	

#else  /* !__SMP__ */

#define EXOS_LOCK(ln)
#define EXOS_SPINLOCK(ln)
#define EXOS_NBLOCK(ln)	
#define EXOS_UNLOCK(ln) 	

#endif /* !__SMP__ */


#endif /* __LOCKS_H__ */

