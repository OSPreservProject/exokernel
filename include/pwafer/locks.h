
#ifndef __PW_LOCKS_H__
#define __PW_LOCKS_H__

#include <pwafer/pwafer.h>
#include <pwafer/kprintf.h>

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


static inline void
prim_spinlock_get (unsigned long addr)
{
  register uint16 content = 1;

test_n_set:
  /* two inputs, two outputs */
  asm volatile ("xchgw %0,%1" :
                "=r" (content),
                "=m" (*(uint16*)addr) :          /* "m" -> don't regalloc */
                "0" (content),                  /* same loc as operand 0 */
                "m" (*(uint16*)addr));
  if (content != 0) 
  {
    while(*((uint16*)addr) != 0) {
      asm volatile ("" : : : "memory");  
        /* this is to tell gcc not to regalloc value of addr */
    }
    goto test_n_set;
  }
}


static inline int
prim_spinlock_try (unsigned long addr)
{
  register uint16 content = 1;

  /* two inputs, two outputs */
  asm volatile ("xchgw %0,%1" :
                "=r" (content),
                "=m" (*(uint16*)addr) :  /* "m" -> don't regalloc */
                "0" (content),                  /* same loc as operand 0 */
                "m" (*(uint16*)addr));
  return -content;  /* return -1 if failed */
}



/*
 * spinlock: trusted spinlock
 */
typedef struct 
{      
  short lock;		/* 0 if unused, 1 otherwise */
  short depth;		/* recursive locking depth */
  u_int owner;		/* owner */
} spinlock_t;

static inline void
spinlock_reset (spinlock_t *l)
{
  l->owner = 0;
  l->depth = 0; 

  /* must do this last */
  l->lock = 0;
}

static inline void
spinlock_acquire (spinlock_t *l)
{
  if (l->owner == getpid())
  {
    l->depth++;
    return;
  }
 
  prim_spinlock_get ((unsigned long)l);

  l->depth++;
  l->owner = getpid();
  return;
}

static inline int
spinlock_try (spinlock_t *l)
{
  if (l->owner == getpid())
  {
    l->depth++;
    return 0;
  }

  if (prim_spinlock_try ((unsigned long)l) < 0)
    return -1;

  l->depth++;
  l->owner = getpid();
  return 0;
}

static inline void
spinlock_release (spinlock_t *l)
{
  l->depth--;

  if (l->depth == 0) 
  {
    spinlock_reset(l);
  }
}



/*
 * yieldlock: trusted yield lock
 */
typedef struct 
{      
  u_int lock;		/* 0 if unused, envid otherwise */
  short depth;		/* recursive locking depth */
  short unused;
} yieldlock_t;

static inline void
yieldlock_reset (yieldlock_t *l)
{
  l->depth = 0; 
  
  /* must do this last */
  l->lock = 0;
}

static inline int
yieldlock_try (yieldlock_t *l)
{
  u_int orig_eid;

  if (l->lock == getpid())
  {
    l->depth++;
    return 0;
  }

  orig_eid = compare_and_swap ((unsigned int *)l, 0, getpid());

  if (orig_eid == 0) /* got the lock */
  {
    l->depth++;
    return 0;
  }

  else /* didn't get the lock */
  {
    return orig_eid;
  }
}

static inline void
yieldlock_acquire (yieldlock_t *l)
{
  int r;
 
again:
  r = yieldlock_try(l);
  if (r > 0)
  {
    yield(r);
    goto again;
  }

  return;
}

static inline void
yieldlock_release (yieldlock_t *l)
{
  l->depth--;

  if (l->depth == 0) 
  {
    yieldlock_reset(l);
  }
}



/*
 * cpulock: grab hold of CPU, disallow context switch temporarily (provide
 * critical section on uniprocessor only).
 */

static inline void
cpulock_reset()
{}

static inline void
cpulock_acquire()
{
  asm ("cmpl $0, %1\n"   /* don't init if we're a nested cpulock */
       "\tjg 1f\n"
       "\tmovl $0, %0\n" /* clear the interrupted flag */
       "\tcall 0f\n"     /* using call to push eip on stack */
       "0:\n"
       "\tpopl %2\n"
       "1:\n"
       "\tincl %1\n"     /* inc count of nested of cpulock */
       : "=m" (UAREA.u_interrupted), 
         "=m" (UAREA.u_in_critical), 
	 "=m" (UAREA.u_critical_eip) :: "cc", "memory");
}

static inline int
cpulock_try()
{
  cpulock_acquire();
  return 0;
}

static inline void
cpulock_release()
{
  // assert(UAREA.u_in_critical > 0);

  asm ("decl %0\n"           /* dec critical ref count */
       "\tcmpl $0, %0\n"     /* if > 0 mean we're still in a critical pair */
       "\tjg 0f\n"
       "\tcmpl $0, %1\n"     /* the last exit critical so yield if needed */
       "\tje 0f\n"           /* not interrupted--no need to yield */
       "\tpushl %2\n"        /* setup our stack to look like an upcall */
       "\tpushfl\n"
       "\tjmp _xue_yield\n"  /* and go to yield */
       "0:\n"
       : "=m" (UAREA.u_in_critical): "m" (UAREA.u_interrupted), "g" (&&next)
       : "cc", "memory");
next:
}


#endif

