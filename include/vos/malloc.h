
/*
 * Copyright 1999 MIT
 */

#ifndef __VOS_MALLOC_H__
#define __VOS_MALLOC_H__

#include <stdio.h>
#include <xok/types.h>

/* feelisthdr: freelist for McKusick-Karels allocator */
typedef struct freelisthdr
{
  u_int next;
} freelisthdr_t;

/* array of freelist headers, recording memory size or location */
extern freelisthdr_t freelistarr[];


#define NUM_FLST 7
#define NDX(size) \
  ((size) > 512 \
    ? (size) > 1024 ? 6 : 5 \
    : (size) > 128  \
      ? (size) > 256 ? 4 : 3 \
      : (size) > 64 \
        ? 2 \
	: (size) > 32 ? 1 : 0)

/* uses McKusick-Karels allocator algorithm, described in Vahalia's book */
#define MALLOC(space, cast, size) \
    { \
      register freelisthdr_t* flh; \
      if (size <= 2048 && (flh = &freelistarr[NDX(size)], flh->next != 0)) \
      { \
	space = (cast)flh->next; \
	flh->next = *(u_int *)space; \
      } \
      else \
      { \
        extern void* imalloc(u_int); \
	space = (cast)imalloc(size); \
      } \
    }


/* free: free a piece of memory */
extern void 
free(void*);


/* malloc: allocate a piece of memory of the given size */
extern inline void*
malloc(u_int size)
{
  void *ptr;
  if (size == 0) 
    return NULL;
  MALLOC(ptr,void*,size);
  return ptr;
}


/* realloc: allocate new memory, copy, free old memory */
extern inline void*
realloc(void* ptr, u_int size);


#endif /* __VOS_MALLOC_H__ */
