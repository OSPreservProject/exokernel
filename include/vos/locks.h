
#ifndef __VOS_LOCKS_H__
#define __VOS_LOCKS_H__

#include <vos/locks_impl.h>

static inline void
__sync_read()
{
  int a;
  asm volatile("lock\n" "\tnotl %0\n" : "=m" (a) : "m" (a));
}

#endif /* __VOS_LOCKS_H__ */

