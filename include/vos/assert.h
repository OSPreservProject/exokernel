
#ifndef __VOS_ASSERT_H__
#define __VOS_ASSERT_H__

#undef assert
#include <stdio.h>

#ifdef __LIBVOS__
#include <vos/kprintf.h>
#include <vos/proc.h>
#define assert(i) \
  if (!(i)) { \
    kprintf("assert failed in %s, line %d: %s\n",__FILE__,__LINE__,#i); \
    exit(-1); \
  }
#else
#define assert(i) \
  if (!(i)) { \
    printf("assert failed in %s, line %d: %s\n",__FILE__,__LINE__,#i); \
    exit(-1); \
  }
#endif

#endif


