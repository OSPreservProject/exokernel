
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

/*
 * Kernel allocation: standard malloc free interface.
 */
#ifndef __KALLOC_H__
#define __KALLOC_H__

struct k_arena; /* Hide details of implementation. */

enum {
        K_CLEAR,                /* clear value for k_errno */
        K_DOUBLE_FREE,          /* node that was freed twice */
        K_BOGUS_FREE,           /* free of bogus (or corrupt) node */
        K_TAIL_CORRUPT,         /* overwrote tail of allocated mem*/
        K_HEAD_CORRUPT,         /* overwrote memory in front of allocated mem */
        K_POST_FREE_CORRUPTION, /* modified a block that had been freed */
        K_UNFREED_MEM,          /* memory was not freed */
        K_SEAL_BROKEN,          /* a software seal was violated */
	K_BOGUS_ARENA,		/* the given arena was not ok. */
};

extern int k_errno; 	/* Set on errors to the above flags. */


/* Allocate memory of sz bytes. */
void *kalloc(unsigned sz);
void *k_a_alloc(struct k_arena *arena, unsigned sz);

/* Free allocated memory p. */
void kfree(void *p);
void k_a_free(struct k_arena *arena, void *p);


/* 
 * Check the integrity of free list (K_FREE_LIST) or allocated 
 * list (K_ALLOC_LIST).  The bitwise or of both flags will check 
 * both lists.
 */
enum { K_FREE_LIST = 16, K_ALLOC_LIST = 32 };
void k_check_integrity(unsigned flag);
void k_a_check_integrity(struct k_arena *a, unsigned flag);


/* 1 = print error messages, 0 = don't print. */
void k_verbose(int verbose_p);

/* 
 * Forcibly free all allocated memory.  Does not currently work on arenas
 * that allow memory to be returned to the client. 
 */
int k_gc(void);
int k_a_gc(struct k_arena *a);

/* Return size of given memory block. */
int k_size(void *p);

/* Increase the size of given memory block. */
void *k_grow(void *p, int bytes);
void *k_a_grow(struct k_arena *a, void *p, int bytes);

/* Shrink the given memory block. */
void *k_shrink(void *p, int sz);
void *k_a_shrink(struct k_arena *a, void *p, int sz);

/* 
 * Allocate a simple, default arena: supports gc, and does not increase
 * memory size.   Arena will allocate memory from mem.
 */
struct k_arena *
k_arena_alloc(void *mem, unsigned sz);
struct k_arena *
k_a_arena_alloc(struct k_arena *ar, void *mem, unsigned sz, int (*get_mem)(void **, int));

/* 
 * More sophisticated arena allocation: allocate space for arena from
 * ar, use get_mem function to grow memory.
 */
struct k_arena *
k_arena_salloc(void *mem, unsigned sz, unsigned attr, int (*get_mem)(void **, int));
struct k_arena * 
k_a_arena_salloc(struct k_arena *ar, void *mem, unsigned sz, unsigned attr, int (*get_mem)(void **, int));

/* Free arena. */
void k_arena_free(struct k_arena *a, void (*give_mem)(void *, int));

/* Check that all memory has been freed.  */
int k_memcheck(void);
int k_a_memcheck(struct k_arena *a);

/* Give memory to allocator. */
void k_return_memory(void *p, int sz);
void k_a_return_memory(struct k_arena *a, void *p, int sz);

/* Determine the space overhead for the given node. */
int k_overhead(int sz);

/* 
 * Coalesce memory in the given arena.   Is then ready to be returned
 * or better allocation.
 */
void k_coalesce(void (*release_memory)(), int cutoff);
void k_a_coalesce(struct k_arena *a, void (*release_memory)(), int cutoff);

/* 
 * Software read protection: Seal a block (computes checksum). Unseal
 * verifies that the checksum is still valid. 
 */
void k_seal(void *data);
void k_unseal(void *data);

/* Replace arena. */
struct k_arena *k_def_arena_swap(struct k_arena *new);

#ifndef NDEBUG
/* 
 * Put these last so they don't screw up prototypes. 
 */
extern char *kalloc_file;
extern int kalloc_line;
#define k_mark (kalloc_line = __LINE__, kalloc_file = __FILE__)

#define kalloc(sz) 	 (k_mark, kalloc(sz))
#define k_a_alloc(a, sz) (k_mark, k_aalloc(a, sz))

#define kfree(p) 	 (k_mark, kfree(p))
#define k_a_free(a, p)   (k_mark, k_afree(a, p))

#define k_check_integrity(flag) 	(k_mark, k_check_integrity(flag))
#define k_a_check_integrity(a, flag) 	(k_mark, k_a_check_integrity(a, flag))

#define k_gc()		(k_mark, k_gc())
#define k_a_gc(a)	(k_mark, k_a_gc(a))

#define k_size(p)	(k_mark, k_size(p))

#define k_grow(p, sz)		(k_mark, k_grow(p, sz))
#define k_a_grow(a, p, sz)	(k_mark, k_a_grow(a, p, sz))

#define k_shrink(p, sz)		(k_mark, k_shrink(p, sz))
#define k_a_shrink(a, p, sz)	(k_mark, k_a_shrink(a, p, sz))

#define k_arena_alloc(mem, sz) 		(k_mark, k_arena_alloc(mem, sz))
#define k_a_arena_alloc(a, mem, sz) 	(k_mark, k_a_arena_alloc(a, mem, sz))

#define k_arena_salloc(mem, sz, attr, get_mem)	\
	(k_mark, k_arena_salloc(mem, sz, attr, get_mem))
#define k_a_arena_salloc(ar, mem, sz, attr, get_mem)	\
	(k_mark, k_a_arena_salloc(ar, mem, sz, attr, get_mem))

#define k_arena_free(a, give_mem)	(k_mark, k_arena_free(a, give_mem))

/* Check that all memory has been freed.  */
#define k_memcheck() (k_mark, k_memcheck())
#define k_a_memcheck(a) (k_mark, k_a_memcheck(a))

#define k_seal(data)	(k_mark, k_seal(data))
#define k_unseal(data)	(k_mark, k_unseal(data))

#define k_def_arena_swap(new) 	(k_mark, k_def_arena_swap(new))

#endif /* NDEBUG */

#endif /* __KALLOC_H__ */
