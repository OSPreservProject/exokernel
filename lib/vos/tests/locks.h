
#ifndef __LOCKS_H__
#define __LOCKS_H__

#include <vos/proc.h>
#include <vos/assert.h>


static inline void
prim_spinlock_get (unsigned long addr)
{
  uint8 content = 1;

test_n_set:
  /* two inputs, two outputs */
  asm volatile ("xchgb %0,%1" :
                "=r" (content),
                "=m" (*(uint8*)addr) :          /* "m" -> don't regalloc */
                "0" (content),                  /* same loc as operand 0 */
                "m" (*(uint8*)addr));
  if (content != 0) 
  {
    while(*((uint8*)addr) != 0) {
      asm volatile ("" : : : "memory");  
        /* this is to tell gcc not to regalloc value of addr */
    }
    goto test_n_set;
  }
}

static inline void
prim_release (unsigned long addr)
{
  *(uint8*)addr = 0;
}


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
  prim_release((unsigned long)l);
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

  
static inline void
spinlock_release (spinlock_t *l)
{
  assert(l->owner == getpid());
  assert(l->lock == 1); 

  if (l->depth <= 0)
  {
    printf("%d: l->depth is %d, %d\n", l->owner, l->depth, l->owner);
    assert(l->depth > 0);
  }

  l->depth--;

  if (l->depth == 0) 
  {
    spinlock_reset(l);
  }
}


#endif

