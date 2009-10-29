
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

#ifndef __MPLOCK_H__
#define __MPLOCK_H__

#include <xok/printf.h>
#include <xok/cpu.h>
#include <xok/mplock_decl.h>

#ifdef KERNEL 


static inline void
atomic_inc(volatile int *dst)
{
  asm volatile ("lock\n" "\tincl %0\n"
                : "=m" (*dst) : "m" (*dst));
}


static inline void
atomic_dec(volatile int *dst)
{
  asm volatile ("lock\n" "\tdecl %0\n"
                : "=m" (*dst) : "m" (*dst));
}


/* read value then increment, all in an atomic sequence */
static inline unsigned short
read_and_inc(volatile unsigned short *dst)
{
  register short one = 1;
  asm volatile ("lock\n" "\txaddw %0,%1\n"
                : "=r" (one), "=m" (*dst) : "0" (one), "m" (*dst));
  return one;
}


/* if fails, return result differs from val, they are the same otherwise */
static inline int
compare_and_swap(volatile int *dst, int val, int new)
{
  int result;
  asm ("lock\n"
       "\tcmpxchgl   %2, %0\n"
       : "=m" (*dst), "=a" (result)
       : "r" (new), "m" (*dst), "a" (val)
       : "eax", "cc", "memory");
  return (result);
}


/* returns -1 if failed, returns 0 otherwise */
static inline int
compare_and_swap_64(volatile int *dst, int v_1, int v_2, int new_v1, int new_v2)
{
  int zero = 0;
  int r;
  asm volatile ("lock\n"
                "\tcmpxchg8b %2\n"
		"\tmovl  $-1,%3\n"
                "\tcmovel %4,%3\n" :
                "=d" (v_2), "=a" (v_1), "=m" (*dst), "=r" (r) :
                "m" (zero), "0" (v_2), "1" (v_1), "b" (new_v1), "c" (new_v2) : 
		"eax", "edx", "cc", "memory");
  return r;
}


/* atomic test and set, returns -1 if failed, returns 0 otherwise */
static inline int
test_and_set (unsigned long addr)
{
  register uint16 content = 1;

  /* two inputs, two outputs */
  asm volatile ("xchgw %0,%1" :
                "=r" (content),
                "=m" (*(uint16*)addr) :  	/* "m" -> don't regalloc */
                "0" (content),			/* same loc as operand 0 */
                "m" (*(uint16*)addr));
  return -content;  /* return -1 if failed */
}


#ifdef __SMP__

static inline void
mplock_non_owner_panic()
{
  panic("trying to release another processor's lock\n");
}

static inline void
mplock_free_lock_panic()
{
  panic("trying to release a free lock\n");
}

static inline void
mplock_illegal_lock_panic()
{
  panic("trying to operate on an illegal lock\n");
}

static inline void
kspinlock_init (struct kspinlock *the_lock)
{
  the_lock->lock = 0;
  the_lock->owner = -1;
  the_lock->uses = 0;
  the_lock->depth = 0; 
}

static inline void
kspinlock_get (struct kspinlock *the_lock)
{
  if (the_lock->owner == cpu_id) 
  {
    the_lock->depth++;
    the_lock->uses++;
    return;
  }

  while (the_lock->lock != 0 || test_and_set((unsigned long)the_lock) < 0)
  {
    /* this is to tell gcc not to regalloc value of addr */
    asm volatile ("" : : : "memory");  

    /* XXX - Hack Alert - check if there is an IPI message pending or not to
     * prevent tlb shootdown deadlock: a processor is holding locks when
     * doing a tlb invalidate over IPI; if other processors are waiting for
     * the lock, they would never process IPI, unless we check for pending
     * IPI messages here... really ought to make IPI a higher priority
     * interrupt than everything else...
     */
    {
      extern void ipi_handler();
      if (in_ipih == 0) ipi_handler();
    }
  }

  the_lock->owner = cpu_id;
  the_lock->depth++;
  the_lock->uses++;
}

static inline int
kspinlock_try (struct kspinlock *the_lock)
{
  if (the_lock->owner == cpu_id) 
  {
    the_lock->depth++;
    the_lock->uses++;
    return 0;
  }
 
  if (test_and_set((unsigned long)the_lock)<0) 
  { /* failed */
    return -1;
  } 
  
  else 
  { /* got the lock */
    the_lock->owner = cpu_id;
    the_lock->depth++;
    the_lock->uses++;
    return 0;
  }
}

static inline int
kspinlock_release (struct kspinlock *the_lock)
{
#ifdef LOCK_DEBUG
  if (the_lock->owner == -1) 
  {
    mplock_free_lock_panic();
    return -1;
  }

  if (the_lock->owner != cpu_id) 
  {
    mplock_non_owner_panic();
    return -1;
  }
#endif

  the_lock->uses++;
  the_lock->depth--;

  if (the_lock->depth == 0) 
  {
    the_lock->owner = -1;
    the_lock->lock = 0; 
  }
  return 0;
}


#define KQUEUELOCK_WAIT 0
#define KQUEUELOCK_FREE 1

static inline void
kqueuelock_init (struct kqueuelock *the_lock)
{
  int i;
  the_lock->lock = 0;
  the_lock->owner = -1;
  the_lock->uses = 0;
  the_lock->depth = 0; 
  for(i=0; i<NR_CPUS; i++) the_lock->tickets[i][0] = KQUEUELOCK_WAIT;
  the_lock->tickets[0][0] = KQUEUELOCK_FREE;
}


static inline void
kqueuelock_get (struct kqueuelock *the_lock)
{
  int myticket;

  if (the_lock->owner == cpu_id) 
  {
    the_lock->depth++;
    the_lock->uses++;
    return;
  }

  myticket = read_and_inc(&(the_lock->lock)) & (NR_CPUS-1);
  ql_tickets[last_ticket] = myticket;
  last_ticket++;

  while(the_lock->tickets[myticket][0] == KQUEUELOCK_WAIT)
  {
    asm volatile("" ::: "memory");
    /* see comments earlier */
    {
      extern void ipi_handler();
      if (in_ipih == 0) ipi_handler();
    }
  }
  the_lock->tickets[myticket][0] = KQUEUELOCK_WAIT;
  the_lock->owner = cpu_id;
  the_lock->depth++;
  the_lock->uses++;
  
  return;
}
    
static inline int
kqueuelock_release (struct kqueuelock *the_lock)
{
  int myticket;

#ifdef LOCK_DEBUG
  if (the_lock->owner == -1) 
  {
    mplock_free_lock_panic();
    return -1;
  }
  if (the_lock->owner != cpu_id) 
  {
    mplock_non_owner_panic();
    return -1;
  }
#endif

  the_lock->uses++;
  the_lock->depth--;

  if (the_lock->depth == 0) 
  {
    last_ticket--;
    myticket = ql_tickets[last_ticket];

    the_lock->owner = -1;
    the_lock->tickets[((myticket+1)&(NR_CPUS-1))][0] = KQUEUELOCK_FREE;
  }
  return 0;
}


static inline void
krwlock_init (struct krwlock *the_lock)
{
  the_lock->nreaders = 0;
  the_lock->writer = -1;
  the_lock->depth = 0;
  the_lock->uses = 0;
}

static inline void
krwlock_read_get (struct krwlock *the_lock)
{
  int nreaders;

retry:
  while(the_lock->writer != -1);
  nreaders = the_lock->nreaders;

  if (compare_and_swap_64
      ((volatile int *)the_lock, nreaders, -1, nreaders+1, -1) < 0)
    goto retry;
  return;
}

static inline int
krwlock_read_release (struct krwlock *the_lock)
{
#ifdef LOCK_DEBUG
  if (the_lock->nreaders == 0)
  {
    mplock_free_lock_panic(); 
    return -1;
  }
  if (the_lock->writer != -1) 
  {
    mplock_illegal_lock_panic();
    return -1;
  }
#endif
  asm volatile 
    ("lock\n\tdecl %0\n" : "=m" (the_lock->nreaders) 
                         : "m"  (the_lock->nreaders));
  return 0;
}

static inline void
krwlock_write_get (struct krwlock *the_lock)
{
  if (the_lock->writer == cpu_id) 
  {
    the_lock->depth++;
    the_lock->uses++;
    return;
  }

  while (compare_and_swap_64((volatile int *)the_lock, 0, -1, 0, cpu_id) < 0);
  
  the_lock->depth++;
  the_lock->uses++;
  return;
}

static inline int
krwlock_write_release (struct krwlock *the_lock)
{
#ifdef LOCK_DEBUG
  if (the_lock->nreaders != 0)
  {
    mplock_illegal_lock_panic();
    return -1;
  }
  if (the_lock->writer != cpu_id) 
  {
    mplock_illegal_lock_panic();
    return -1;
  }
#endif
  the_lock->uses++;
  the_lock->depth--;

  if (the_lock->depth == 0) 
    the_lock->writer = -1;

  return 0;
}



#define MP_SPINLOCK_INIT(l)				\
    kspinlock_init(l)
#define MP_SPINLOCK_GET(l)				\
  if (smp_commenced)					\
    kspinlock_get(l)
#define MP_SPINLOCK_RELEASE(l)				\
  if (smp_commenced)					\
    kspinlock_release(l)

#define MP_QUEUELOCK_INIT(l)				\
    kqueuelock_init(l)
#define MP_QUEUELOCK_GET(l) 				\
  if (smp_commenced) 			                \
    kqueuelock_get(l);
#define MP_QUEUELOCK_RELEASE(l) 			\
  if (smp_commenced) 					\
    kqueuelock_release(l);

#define MP_RWLOCK_INIT(l)				\
    krwlock_init(l)
#define MP_RWLOCK_READ_GET(l)				\
  if (smp_commenced)					\
    krwlock_read_get(l);
#define MP_RWLOCK_READ_RELEASE(l)			\
  if (smp_commenced)					\
    krwlock_read_release(l);
#define MP_RWLOCK_WRITE_GET(l)				\
  if (smp_commenced)					\
    krwlock_write_get(l);
#define MP_RWLOCK_WRITE_RELEASE(l)			\
  if (smp_commenced)					\
    krwlock_write_release(l);

#else 

#define MP_SPINLOCK_INIT(l)
#define MP_SPINLOCK_GET(l)
#define MP_SPINLOCK_RELEASE(l)
#define MP_QUEUELOCK_INIT(l)
#define MP_QUEUELOCK_GET(l)
#define MP_QUEUELOCK_RELEASE(l)
#define MP_RWLOCK_INIT(l)
#define MP_RWLOCK_READ_GET(l)
#define MP_RWLOCK_READ_RELEASE(l)
#define MP_RWLOCK_WRITE_GET(l)
#define MP_RWLOCK_WRITE_RELEASE(l)


#endif /* SMP */
#endif /* KERNEL */

#endif /* __MPLOCK_H__ */


