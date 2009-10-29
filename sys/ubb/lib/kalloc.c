
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
 * Fast/space-efficient memory allocator.  Designed for use in the exokernel.
 * Has a lot of sanity checks available.
 *
 * Each memory block is laid out as follows:
 *
 *   +--------+-----------------+------+------------------+-------------------+
 *   | header | head fence post | data | unused remainder | tail fence post   |
 *   +--------+-----------------+------+------------------+-------------------+
 * Remainders arise when a memory request for a given size is satisfied with 
 * a larger block.  ``remainder + tail fence post'' can be derived using
 * block_sz - data_sz - sizeof(node) - FENCE_SZ;
 *
 * The ifdef's seem to be the safest route: without them it would be
 * easy to use undefined structure fields.
 *
 * A lot of these routines could check their arguments.  Should their
 * return type all be changed to int?
 */

#define DDEBUG


#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include "demand.h"

/* need this for cpp trickery. */
#ifdef NDEBUG
#	include "kalloc.h"
#else
#	define NDEBUG
#	include "kalloc.h"
#	undef NDEBUG
#endif

/* Can arbitrarily return memory or supports GC. */
enum { K_RET_MEM, K_GC_MEM };

/* 
 * Arenas are good to:
 *	1. make sure memory that is allocated is contigious
 *	2. specialize allocation policies.
 * 	3. free all at once.
 */

#define roundup(x,n) (((x)+((n)-1))&(~((n)-1)))
/* Is the given size too small to even be a node? */
#define TOO_SMALL_P(x) ((x) < compute_sz(0))

/*
 * Number of chunks to allocate at once.  Numbers close to one trade
 * time for space; farther trade space for time.
 */
#define K_NCHUNKS	4
#define K_FENCE_COOKIE	0xfe	/* iron makes you strong */
#define K_DATA_COOKIE	0xce	
#define K_CUTOFF	64	/* fast access blocks */
#define K_HEAP_INCR	4	/* number of entries we grow heap by */
#define K_MAGIC		0xfeedface
#define K_ARENA_COOKIE 0xbabeBABE

/* mark next pointer of allocated nodes. */
#define K_ALLOCATED_FLAG	(void *)0x12345678	
#define BOGUS_POINTER	((void *)0x4)
#define ALIGN_MULTIPLE	4

#ifdef DDEBUG
#	define FENCE_SZ 	(8 * sizeof(int))
#else
#	define FENCE_SZ 	0
#endif


/* The basic structure.  Currently, is way big. NOTE: must be a multiple
 * of ALIGN_MULTIPLE! */
struct node {
	/*
	 * This is the current ``essential'' information.  We could easily
	 * reduce it to 4 bytes (or less, if we use zones to track sizes)
	 * but I'm not sure it is worth the present effort.
	 */
	struct node *next;	/* pointer to next in free list */
	unsigned int     block_sz; 	/* actual size of block */

#ifdef DDEBUG
	struct node *alloc_prev;	/* previous node on allocated list */
	struct node *alloc_next; /* next node on allocated list */

	/*
	 * With debugging off, the rest of this structure is not
	 * used.  We pretend it is there to avoid death by union.
	 */
	unsigned int     data_sz;	/* requested size */
	int	free;		/* boolean set to prevent double frees */
	int 	magic;		/* a magic cookie used to tag nodes */
	char   *freed_file;	/* file name where block was freed */
	char   *alloc_file;	/* file name where block was allocated */
	unsigned short     freed_line;	/* line where block was freed */
	unsigned short     alloc_line;	/* line where block was allocated */
	unsigned short	   sealed_p;	/* is the node sealed? */
	unsigned short	   chk_sum;	/* node checksum */
	char	pad[4];
#endif
};

/*  
 * Memory allocation arena: isolates where memory is allocated from. 
 * It is currently too primitive in that it does not attempt to acquire
 * more memory when it runs out.  We should track legal arena; we settle
 * for a cookie.
 */
struct k_arena {
	/* 
	 * Because we statically initialize the default_arena, it 
	 * is important that these 4 fields stay in their current
	 * position.  If they move, update the initialization.
	 */
	int (*get_mem)(void **, int); /* pointer to routine returning memory. */
	unsigned cookie;		/* a cookie to ensure integrity. */
	struct k_arena *parent;		/* parent that we pull memory out of */
	struct node *heapv;		/* list of all entries on the heap. */
	unsigned hsz;			/* total number of heap entries */
	unsigned hn;			/* number of used heap entries */
	unsigned method;		/* Support gc or not? */
	unsigned nbytes;		/* totoal number of bytes allocated. */

	struct node *fl[K_CUTOFF];	/* the free list for this arena */

	/* 
	 * List blocks whose size is too large for the freelist. 
	 * Sorted by size.
	 */
	struct node *jumbo_blocks;
#ifdef DDEBUG
	unsigned nalloced; 		/* number of allocated blocks. */
	struct node *alloced;		/* pointers to allocated blocks */
#endif
};

/* 
 * Iterator for heap: 
	For every element in heap: 
		set current element to name
		run code
 */
#define HEAP_ITER(arena, name, code) do {			\
	struct node *h, *end;					\
	h = &a->heapv[0]; end = &h[a->hn]; 			\
	for(name = h; name < end; name++) {			\
		code;						\
	}							\
} while(0)

/* 
 * Defining the default allocation arena where memory will be taken
 * from for non-arena requests.  double should equal maximum alignment.
 */
static long long default_heap[4096*24];
/*
 * This is a hack to ensure that we have a heap structure.  However,
 * we are going to have to rethink where to get the memory from to
 * do the heapv allocation correctly.  Maybe allocate a reserve on
 * every allocation & consider it for elimination during coalescing.
 */
static struct node default_heapv[1];

static int default_grab_memory(void **p, int sz); 

/* The default arena itself. */
static struct k_arena default_arena = 
   { default_grab_memory, K_ARENA_COOKIE, 0, default_heapv, 1, 0, K_GC_MEM};

/* We keep the pointer around for testing: easy to swap the current arena. */
static struct k_arena *def_arena = &default_arena;

static struct k_arena * import_arena(struct k_arena *a);
static inline unsigned compute_sz(unsigned sz);
static unsigned compute_index(unsigned sz);
static void k_error(char *msg, int errno, struct node *p);
static unsigned short inet_cksum(unsigned short *addr, unsigned short count);

static int return_nul(void **p, int sz);
static void *alloc(struct k_arena *a, unsigned sz, char *file, int line);
static void dealloc(struct k_arena *a, void *p, int sz, char *file, int line);
static inline struct node * import_data(void *p);
static inline struct node *data_to_head(void *data);
static inline void initial_enqueue(struct k_arena *a, void *q, int sz);

static void heap_enqueue(struct k_arena *a, void *mem, int sz);
static inline int support_gc(struct k_arena *a);
static void not_qsort (void *array, unsigned count, unsigned size, int (*compare)());

#ifdef DDEBUG
/*
 * The following four functions allow each piece of a memory block 
 * to be reached.
 */
static inline void *head_to_fence1(struct node *n);
static inline void *head_to_data(struct node *head);
static inline void *head_to_fence2(struct node *head);

/* Sanity checks. */
static int cmpchr(char *p, char chr, int sz);
static void chk_node(struct node *p, int check_data_p);
#endif


/************************************************************************
 * Exported allocation interface.
 */

/* Used to record the file and line that allocation/deallocation happened at */
char   *kalloc_file;
int     kalloc_line;
int	k_errno;
static int k_verbose_p = 1;	/* whether error messages should be printed */


/*
 * Should add some routines that align their arguments; also, arenas
 * would be good.  Steal from lcc?  Add functions that do not main
 * tain a next pointer.  Should provide a k_size fucntion that returns
 * the size of an allocated block.  Also, could provide a k_valid function
 * that clients can use to determine valid pointers.
 */

/* Kernel arena allocation.  */
void   *
k_a_alloc(struct k_arena *arena, unsigned sz)
{
	return alloc(arena, sz, kalloc_file, kalloc_line);
}

/* Kernel allocation: standard malloc free interface.  */
void   *
kalloc(unsigned sz)
{
	return k_a_alloc(def_arena, sz);
}

/* 
 * Kernel arena free.
 */
void 
k_a_free(struct k_arena *arena, void *p)
{
	/* Should we flag a null pointer? */
	if(p)
		dealloc(arena, p, 0, kalloc_file, kalloc_line);
}

/* 
 * Kernel free: standard free semantics.
 */
void 
kfree(void *p)
{
	k_a_free(def_arena, p);
}


/*
 * Set verbosity.  Determines whether error messages are printed
 * or k_errno is relied on to propogate problems.
 */
void
k_verbose(int verbose_p) 
{
	k_verbose_p = verbose_p;
}

/* Hack for testing. */
int 
k_sizeof_arena(void) 
{
	return sizeof default_heap;
}
int 
k_fence_sz(void) 
{
	return FENCE_SZ;
}


/*
 * Free all the allocated memory in a given arena.
 */
int
k_a_gc(struct k_arena *a) 
{
	struct node *p, *next;

	if(!(a = import_arena(a)))
		return 0;
	if(!support_gc(a))
		return 0;

#	ifdef DDEBUG
		for(p = a->alloced; p; p = next) {
			next = p->alloc_next;
			kfree(head_to_data(p));
		}
		a->alloced = 0;
		a->nalloced = 0;
#	endif

	/* Initialize these before enqueuing blocks! */
	a->jumbo_blocks = 0;
	memset(a->fl, 0, sizeof a->fl);

	/* Go through the heap list and do an initial enqueue. */
	HEAP_ITER(a, p, { initial_enqueue(a, p->next, p->block_sz); });
	return 1;
}

/*
 * Free all memory in the default arena.
 */
int
k_gc(void)
{
	return k_a_gc(def_arena);
}

int
k_size(void *p) {
	struct node *n;
	if(!(n = import_data(p)))
		return -1;
	return n->block_sz;
}

/* Grow memory block p by bytes. */
void *
k_a_grow(struct k_arena *a, void *p, int bytes) {
	int n;
	void *q;

	if(!a)
		return 0;	/* should we return error codes? */
	else if(!p) 
		n = 0;
	else if((n = k_size(p)) < 0)
		return 0;

	if(n > bytes)		/* don't shrink. */
		return 0;
       	if(!(q = k_a_alloc(a, bytes)))
		return 0;
	memcpy(q, p, n);
	k_a_free(a, p);

	return q;
}

void *
k_grow(void *p, int bytes) 
{
	return k_a_grow(def_arena, p, bytes);
}

/* Shrink a memory block.  Make this more useful. */
void *
k_a_shrink(struct k_arena *a, void *p, int sz)
{
	return p; /* do nothing: should realloc and move. */
}

void *
k_shrink(void *p, int sz)
{
	return k_a_shrink(def_arena, p, sz);
}

/* 
 * Allocate a arena.  It is provided by the client with memory to manage.
 * When this memory runs out, it returns an error code.  More memory can
 * be provided by using the k_grow_arena function.
 * Note: this function can be used with statically allocated memory 
 * or using kalloc'ed memory.   It would be nice to allow it to use a
 * statically allocated arena function, but this would require us
 * to export the arena structure definition.  Should allow an arena
 * to be specified to allocate the memory from (also, could just take
 * a possibly nul pointer).
 */

/* 
 * Sophisticated arena allocation.  Allow K_GC_MEM or K_RET_MEM
 * to be set and a function supplied to grow memory when need
 * be as well as allowing control over where the arena is allocated from.
 */
struct k_arena *
k_a_arena_salloc(struct k_arena *ar, void *mem, unsigned sz, unsigned attr, int (*get_mem)(void **, int))
{
	struct k_arena *a;

	if(attr != K_RET_MEM && attr != K_GC_MEM)
		return 0;
	if(!(ar = import_arena(ar))) 
		return 0;

	if((a = k_a_alloc(ar, sizeof *a))) {
		memset(a, 0, sizeof *a);
		a->cookie = K_ARENA_COOKIE;
		a->get_mem = get_mem ? get_mem : return_nul;
		a->parent = ar;
		/* Set whether we need to support a heap or not. */
		a->method = attr;
		
		/* enqueue the first node: do this last otherwise the 
		 * arena won't be initialized correctly.  */
		heap_enqueue(a, mem, sz);
		/* enqueue the memory for consumption. */
		initial_enqueue(a, mem, sz);
	}
	return a;
}

struct k_arena *
k_arena_salloc(void *mem, unsigned sz, unsigned attr, int (*get_mem)(void **, int))
{
	return k_a_arena_salloc(def_arena, mem, sz, attr, get_mem);
}

/* 
 * For sugar, we provide a few curried versions of arena allocation.
 * Basically there are two incompatible arenas.  First, arena's that
 * track all allocated memory so that it can be deallocated in one
 * fell swoop.  Second, arena's that support return of memory back
 * to the client.  There is no fundamental reason that these are
 * not unified, other than a small matter of programming.
 */

/* Default: support garbage-collection, no return of memory. */
struct k_arena *
k_arena_alloc(void *mem, unsigned sz) {
	return k_arena_salloc(mem, sz, K_GC_MEM, 0);
}

struct k_arena *
k_a_arena_alloc(struct k_arena *ar, void *mem, unsigned sz, int (*get_mem)(void **, int))
{
	return k_a_arena_salloc(ar, mem, sz, K_GC_MEM, 0);
}


/* Deallocate an arena; returns the original base pointer given. */
void
k_arena_free(struct k_arena *a, void (*give_mem)(void *, int)) {
	struct node *p;

	/* need to make sure all memory in the arena is deallocated? */
	if(!import_arena(a)) {
		if(k_verbose_p)
			printf("k_free_arena: bogus arena\n");
		return;
	
	} 
	if(give_mem) {
		if(support_gc(a))
			/*
			 * For gc arenas, just free every piece of the
			 * heap. 
			 */
			HEAP_ITER(a, p, { give_mem(p->next, p->block_sz); });
		else {
			/* 
			 * For non-gc arenas, coalesce memory and then 
	 		 * traverse the freelist. 
			 */
			k_a_coalesce(a, give_mem, 0);
		}
	}

	k_a_free(a->parent, a->heapv);
	k_a_free(a->parent, a);
}


/*
 * Check that all memory in given arena has been dealloced.  With debugging
 * off, always returns 0 bytes not-freed.
 */
int
k_a_memcheck(struct k_arena *a)
{
	struct node *p;
	int unfree;

	if(!(a = import_arena(a)))
		return 0;
	unfree = 0;

#	ifdef DDEBUG
		for(p = a->alloced; p; p = p->alloc_next, unfree++) {
			demand(!p->free, freed block should not be on alloced list);
			k_error("k_memcheck: not freed", K_UNFREED_MEM, p);
		}

		demand(unfree == a->nalloced, bogus allocation count);
#	endif

	/* return the number of entries that are outstanding. */
	return unfree;
}

/*
 * Check that all memory in default arena has been dealloced.
 */
int
k_memcheck(void)
{
	return k_a_memcheck(def_arena);
}

/*
 * We isolate the fact that we have to do page allocations from
 * kalloc.  This allows us to strip the restriction out when 
 * it does not apply.  Coalescing is currenlty synchronous in that it
 * is initiaed by a call to coalesce by the client program.  
 * Returning memory back to the client has a fragmentation and
 * function duplication problem.  We solve this using two functions:
 * first, kalloc calls release_memory with whatever blocks it has.
 * release memory determines what pieces to take if any and then
 * calls return_memory with any pieces it cannot reclaim.  This
 * way it doesn't have to track pieces on the ends of blocks that
 * it deallocates.
 */

/*
 * Release_memory should have the potential of being an asynchronous call 
 * for when the allocator is not doing very much.  Most cases, however,
 * higher-level software probably knows when it should be invoked.
 */

/* 
 * Return p to the pool of memory.  Must ensure that it is of the 
 * correct alignment.
 */
void 
k_a_return_memory(struct k_arena *a, void *p, int sz) 
{
	/* Enqueue the block on the freelist. */
	initial_enqueue(a, p, sz);
}

void 
k_return_memory(void *p, int sz) 
{
	k_a_return_memory(def_arena, p, sz);
}

/* Compute overhead for given size. */
int 
k_overhead(int sz)
{
	return compute_sz(sz);
}


/* 
 * Called to coalesce memory.  If release_memory is non-nil it will
 * be called with each block to be released.  For improved efficiency
 * it is only called if the block is of a given size.  
 *
 * NOTE: we have to ensure that k_a_return_memory can be called
 * when this routine is active.  For instance, the freelist 
 * should not be left in an inconsistent state.

 * Seperate this out into a seperate shrink routine.
 */
		/* Used to compare pointers by quicksort. */
		static int cmp(const void *p, const void *q) {
			return *(struct node **)p - *(struct node **)q;
		}
		/* Are two nodes contigious? */
		static int coalesce_p(void *p, int sz, void *q) {
			return ((char *)p + sz) == q;
		}

void k_a_coalesce(struct k_arena *a, void (*release_memory)(), int cutoff) {
	int n, nbytes;

	/* 
	 * Sort the freelist and coalesce it.  It is a bit dicey
	 * that we allocate a contigious memory vector for all blocks.
	 * Could cause kernel stack overflow.  Probably want to use
	 * dynamic allocation, but will have to be careful of deadlock. 
 	 */

demand(0,big heap good code);
	/* Compute largest number of blocks that could be on freelist. */
	nbytes = a->nbytes;
	n = nbytes / compute_sz(0) + 1;
	{
		struct node *list[n]; /* could easily have a stack overflow. */
		struct node *p, *q, **l, **fl; 
		int i;

		/* accumulate all free blocks */
		for(fl = &a->fl[0], l = &list[0], i = 0; i < K_CUTOFF; i++)
			/* note, we reinit fl to null. */
			for(p = fl[i], fl[i] = 0; p; p = p->next, l++)
				*l = p;

		/* Now do the jumbo_blocks */
		for(p = a->jumbo_blocks; p; p = p->next, l++)
			*l = p;
		a->jumbo_blocks = 0;	/* reset the pointer */

		/* sort them */
		demand( n > (l - &list[0]), Vector was too small!);
		n = l - &list[0];
		not_qsort(&list[0], n, sizeof list[0], cmp);

		/* 
		 * Hack: we nul terminate the list so that the coalescing
		 * code does not have to special case the last element.
		 */
		list[n] = 0;

		/* coalesce & return them. */
		for(i = 0; i < n; i++) {
			p = list[i];
			q = list[i+1];
			/* they are contigious */
			if(coalesce_p(p, p->block_sz, q)) {
 				/* coalesce them */
				p->block_sz += q->block_sz;
				list[i+1] = p;
			} else {
				/* 
				 * We don't return memory if:
				 * 1. it isn't large enough.
				 * 2. we don't have a procedure.
				 * 3. the arena supports garbage collection.
				 * 	- to get back memory from a gc arena,
				 *	  gc it, then free.
				 */
				if(p->block_sz < cutoff 
				|| !release_memory 
				|| support_gc(a))
					k_a_return_memory(a, p, p->block_sz);
				/* otherwise, attempt to return back to user. */
				else {
					/* to do this, will have to add support
					 * to the heap to do a byte count on
					 * every heap block && see how many 
					 * bytes are in use.  something like
					 * this eliminates the need for 
					 * release memory with some overhead.
					 */
					demand(0, not hanlding this case);
					a->nbytes -= p->block_sz;
					release_memory(p);
				}
			}
		}
		/* done! */
	}
}

void 
k_coalesce(void (*release_memory)(), int cutoff) {
	k_a_coalesce(def_arena, release_memory, cutoff);
}


/*
 * Check that memory in given arena is ok.
 */
void
k_a_check_integrity(struct k_arena *a, unsigned flag)
{
	struct node *p;
	int unfree, i;

	if(!(a = import_arena(a)))
		return;

#	ifdef DDEBUG
		if(flag & K_ALLOC_LIST) {
			for(unfree = 0, p = a->alloced; p; p = p->alloc_next, unfree++)
				chk_node(p, 0);
			demand(unfree == a->nalloced, bogus allocation count);
		}
		if(flag & K_FREE_LIST) {
			for(i = 0; i < K_CUTOFF; i++) {
				for(p = a->fl[i]; p; p = p->next)
					chk_node(p, 1);
			}
			for(p = a->jumbo_blocks; p; p = p->next)
				chk_node(p, 1);
		}
#	endif
}

/*
 * Check that memory in default arena is ok.
 */
void
k_check_integrity(unsigned flag)
{
	k_a_check_integrity(def_arena, flag);
}


/* Seal the data (i.e., enforce read-only protection in software). */
void 
k_seal(void *data) {
	struct node *n;

	/* make sure it is safe */
	if(!(n = import_data(data)))
		return;
#	ifdef DDEBUG
		n->chk_sum = 0;
		n->sealed_p = 1;
		n->chk_sum = inet_cksum((void *)n, n->block_sz);
		demand(inet_cksum((void *)n, n->block_sz) == 0, bogus checksum);
#	endif
}

/* 
 * Unseal data (i.e., remove read protection). Does nothing with debugging
 * off.
 */
void
k_unseal(void *data) {
	struct node *n;

#	ifdef DDEBUG
		/* make sure it is safe */
		if(!(n = import_data(data)))
			return;
		else if(!n->sealed_p)
			k_error("k_unseal: seal was not set", K_SEAL_BROKEN, n);
		else if(inet_cksum((void *)n, n->block_sz))
			k_error("k_unseal: seal violated", K_SEAL_BROKEN, n);
		else
			n->sealed_p = 0;
#	endif
}

/* Replace default arena. */
struct k_arena *
k_def_arena_swap(struct k_arena *new) {
	struct k_arena *old;

	/* Should check for integrity? */
	old = def_arena;
	def_arena = new;
	return old;
}



/**************************************************************************
 * Internal routines.
 */

#ifdef DDEBUG
static inline void *
head_to_fence1(struct node *n) 
{
	/* skip over the header */
	return (void *)((char *)n + sizeof *n);
}


static inline void *
head_to_fence2(struct node *head) 
{
	/* skip over the header, the first fence post and the data */
	return (void *)((char *)head + sizeof *head + FENCE_SZ + head->data_sz);
}

static inline int
fence2_size(struct node *head) {
	/* 
 	 * fence2 size is equal to 
	 * 	total size - (sizeof head + size of first fence + size of data)
	 */
 	return head->block_sz - (sizeof * head + FENCE_SZ + head->data_sz); 
}

/* 
 * Compare chr to all of p.  Returns -1 on success, index of error otherwise. 
 */
static int 
cmpchr(char *p, char chr, int sz)
{
	int     i;

	for (i = 0; i < sz; i++)
		if (p[i] != chr)
			return i;
	return -1;
}

/* Given the head pointer, check the first and second fence posts */
static void
chk_node(struct node *p, int check_data_p)
{
	if (cmpchr(head_to_fence1(p),  K_FENCE_COOKIE, FENCE_SZ) >= 0)
		k_error("Head fencepost: data corrupted", K_HEAD_CORRUPT, p);
	if (cmpchr(head_to_fence2(p), K_FENCE_COOKIE, fence2_size(p)) >= 0)
		k_error("Tail fencepost: data corrupted", K_TAIL_CORRUPT, p);
	if(!check_data_p)
		return;
	if (cmpchr(head_to_data(p), K_DATA_COOKIE, p->data_sz) >= 0)
		k_error("Post-free fencepost corrupted", 
						K_POST_FREE_CORRUPTION, p);
}
#endif


static int return_nul(void **p, int sz) { *p = 0; return 0; }

/* Do not support garbage collection when returning memory. */
static inline int
support_gc(struct k_arena *a)
{
	return a->method == K_GC_MEM;
}


/* Enqueue a block of memory on arena a's heap list. */
static void
heap_enqueue(struct k_arena *a, void *mem, int sz) {
	struct node *p;

	if(!support_gc(a))
		return;

	demand(!TOO_SMALL_P(sz), not handling this case!);

	/* need more space? */
	if(a->hn >= a->hsz) {
		a->hsz += K_HEAP_INCR;
		a->heapv = k_a_grow(a->parent, a->heapv, a->hsz);
		/* Uhoh. */
		demand(a->heapv, Not enough memory to allocate heap pointers!);
	}
	a->nbytes += sz;

	/* Track what memory we have been allocated. */
	p = &a->heapv[a->hn++];
	p->next = mem;
	p->block_sz = sz;
}

/* have to subtract off the head fence and node size to get it. */
static inline struct node *
data_to_head(void *data) 
{
	return (void *)((char *)data - FENCE_SZ - sizeof(struct node));
}

static inline void *
head_to_data(struct node *head) 
{
	/* skip over the header and the first fencepost */
	return (void *)((char *)head + sizeof *head + FENCE_SZ);
}

/*
 * Given a size, compute what memory block size is actually required,
 * taking into account the need to store tags, fenceposts, etc.
 */
static inline unsigned 
compute_sz(unsigned sz)
{
	/* Don't allocate units of less than void * alignment. */
	sz = roundup(sz, sizeof(void *));

	/* allocate extra space for fence-posts */
	sz += FENCE_SZ * 2;

	/* allocate enough space for header */
	sz += sizeof(struct node);

	sz = roundup(sz, ALIGN_MULTIPLE);
	return sz;
}

/*
 * ALIGN_MULTIPLE alignment is the minimum, therefore we divide to make better use
 * of freelist space.
 */
static inline unsigned 
compute_index(unsigned sz)
{
	return sz / ALIGN_MULTIPLE;
}


/* Report error.  Used in kernel, so we don't use stderr. */
static void
k_error(char *msg, int errno, struct node *p)
{
	k_errno = errno;

	if(!k_verbose_p)
		return;

	printf("%s %d: %s\n", kalloc_file, kalloc_line, msg);

	if(!p)
		return;
#	ifdef DDEBUG
		printf("memory %p, %d allocated at (%s, %d), freed at (%s, %d)\n",
	  		head_to_data(p), p->data_sz, p->alloc_file, 
			p->alloc_line, p->freed_file, p->freed_line);
#	endif
}

/* 
 * Given a piece of user-data, make (reasonably) sure it was something that we
 * allocated.
 */
static inline struct node *
import_data(void *p) 
{
	struct node *n;

       	n = data_to_head(p);

#	ifdef DDEBUG
		if(n->magic != K_MAGIC) {
                	k_error("Free of bogus data or corrupted node.\n", K_BOGUS_FREE, 0);
                	return 0;
        	}
        	/* It had a cookie; hopefully is a valid node. */
        	chk_node(n, 0);
#	endif

	/* check that next flag was correctly deallocated. */
	if(n->next != K_ALLOCATED_FLAG){
                k_error("Double free or corrupted node.\n", K_DOUBLE_FREE, 0);
                return 0;
	}

	return n;
}

/* export a data block. */
static inline void *
export_data(struct node *p) {
	p->next = K_ALLOCATED_FLAG;
	return head_to_data(p);	/* get data */
}

/*
 * Could seal/unseal.
 */
static struct k_arena *
import_arena(struct k_arena *a) {
	if(a && a->cookie == K_ARENA_COOKIE) 
		return a;
	else {
		k_errno = K_BOGUS_ARENA;
		return 0;
	}
}


/* 
 * Dequeue an element of at least req_sz from the free list.  If no
 * element is exactly req_sz, then we traverse the list looking for
 * larger blocks.  Return null if there is nothing larger.
 * We should consider splitting the node: the current allocation
 * policy has really bad worst-case behavior.
 */
static inline struct node *
dequeue(struct k_arena *a, unsigned req_sz, unsigned sz, char *file, int line)
{
	struct node *p, *prev, *q;
	unsigned index;

	p = 0;
	if(compute_index(sz) < K_CUTOFF) {
		/* walk the list looking to find a block of the required size */
		for (p = 0, index = compute_index(sz); index < K_CUTOFF; index++) {
			if ((p = a->fl[index])) {
				a->fl[index] = p->next;	/* dequeue */
				break;
			}
		}
	}
	/* If compute_index was larger or p couldn't be found. */
	if(!p) {
		/* walk list of jumbo blocks looking for a match. */
		for(prev = 0, p = a->jumbo_blocks; p; prev = p, p = p->next) {
			if(p->block_sz >= sz) {
				/* dequeue */
				if(prev) 
					prev->next = p->next;
				else
					a->jumbo_blocks = p->next;
				break;
			}
		}
	}

	/* Nothing there. */
	if(!p)
		return 0;

#	ifdef DDEBUG
		/* Check that memory was not corrupted since we freed it. */
		chk_node(p, 1);

		/* track where allocated from */
		p->alloc_file = file;
		p->alloc_line = line;
		p->freed_file = "Not Freed";
		p->freed_line = 0;
		p->data_sz = req_sz;
		demand(p->free, node should be free);
		p->free = 0;

		/* now set the remainder (if any) */
		memset(head_to_fence2(p), K_FENCE_COOKIE, fence2_size(p));
		/*
	 	* Enqueue on allocated list.
 	 	*/
		if((p->alloc_next = a->alloced))
			a->alloced->alloc_prev = p;
		p->alloc_prev = 0;
		a->alloced = p;
		a->nalloced++;
#	endif

	/* 
	 * If there is enough memory left over, split the node. 
 	 * Currently we do this if the amount allocated is less
	 * than or equal to the amount leftover.
	 */
	if(sz <= (p->block_sz - sz)) {
		/* split it. */
		q = (void *)((char *)p + sz);
		q->block_sz = p->block_sz - sz;
		p->block_sz = sz;
		initial_enqueue(a, q, q->block_sz);
	}

	return export_data(p);
}

/* enqueue an element onto the freelist. */
static inline void
enqueue(struct k_arena *a, struct node *p, char *file, int line)
{
	unsigned index;

#	ifdef DDEBUG
		demand(!p->free, Free must be set);

		/* Attempt to detect and prevent access after free */
		memset(head_to_data(p), K_DATA_COOKIE, p->data_sz);
		memset(head_to_fence1(p), K_FENCE_COOKIE, FENCE_SZ);
		memset(head_to_fence2(p), K_FENCE_COOKIE, fence2_size(p));
	
		/* mark where freed */
		p->freed_file = file;
		p->freed_line = line;
		p->free = 1;
		p->sealed_p = 0;
#	endif

	/* put block on the right freelist. */
	if((index = compute_index(p->block_sz)) < K_CUTOFF) {
		/* enqueue */
		p->next = a->fl[index];
		a->fl[index] = p;
	} else {
		struct node *prev, *q;

		for(prev = 0, q = a->jumbo_blocks; q; prev = q, q = q->next)
			if(p->block_sz <= q->block_sz)
				break;
		if(!prev) {
			p->next = a->jumbo_blocks;
			a->jumbo_blocks = p;
		} else {
			p->next = prev->next;
			prev->next = p;
		}
	}
}

static inline void
initial_enqueue(struct k_arena *a, void *q, int sz) {
	struct node *p;

	p = q;
	demand(sz >= 0, Internal error: bogus size);
#if 0
#endif
	if((unsigned)p % ALIGN_MULTIPLE)
		demand((unsigned)p % ALIGN_MULTIPLE == 0, Bogus alignment);

#	ifdef DDEBUG
		p->alloc_file = "Initial allocation";
		p->alloc_line = 0;
		p->free = 0;
		p->magic = K_MAGIC;
		p->data_sz = 0;
#	endif

	p->block_sz = sz;
	enqueue(a, p, "Initial allocation", 0);
}

/*
 * Allocate memory of size sz.  we round up and add some alignment to
 * this.
 */
static void *
alloc(struct k_arena *a, unsigned sz, char *file, int line)
{
	struct node *p;
	unsigned req_sz;
	int     i, got, request;
	void *q;

	if (!sz)
		return 0;

	/* the asked for size (used to setup fenceposts) */
	req_sz = sz;

	/* actual size we need for record keeping, etc. */
	sz = compute_sz(sz);

	/* Fast path: we have a block in place */
	if ((p = dequeue(a, req_sz, sz, file, line)))
		return (void *) p;

	/*
	 * Didn't find one; first try to grab more memory
	 * (set a threshold for this?).  If this fails, 
	 * coalesce available memory.  If this failts,
	 * return nul.
	 */
	
	request = sz * K_NCHUNKS;
	if(compute_index(sz) < K_CUTOFF 
	&& (got = a->get_mem(&q, request))) {
		char *ptr;

		/* enqueue each chunk */
		for (ptr = q, i = 0; i < K_NCHUNKS; i++, ptr += sz)
			initial_enqueue(a, ptr, sz);
		/* append the tail */
		if(got != request)
			initial_enqueue(a, ptr, got - request);
		/* DO this last to break circularity. */
		heap_enqueue(a, q, got);
	/* If we can't allocate K_NCHUNKS, try for one. */
	} else if((got = a->get_mem(&q, sz))) {
		initial_enqueue(a, q, sz);

		/* append the tail */
		if(got != sz)
			initial_enqueue(a, (char *)q + sz, got - sz);
		/* Do this last to break circularity. */
		heap_enqueue(a, q, got);
	/* If we can't get one, coalesce and try one more time. */
	} else {
		k_a_coalesce(a, 0, 0);
		return dequeue(a, req_sz, sz, file, line);
	}
	p = dequeue(a, req_sz, sz, file, line);
	demand(p, internal error:just allocated space);
	return p;
}

/* Free memory of sz.  */
static void 
dealloc(struct k_arena *a, void *p, int sz, char *file, int line)
{
	struct node *n, *prev, *next;

	if(!(n = import_data(p)))
		return;

#	ifdef DDEBUG
		/* Take off of the allocated list. */
		next = n->alloc_next;
		prev = n->alloc_prev;

		if(n->free) {
			k_error("Double free or corrupted node", K_DOUBLE_FREE, n);
			return;
		} else if(next == BOGUS_POINTER) {
			k_error("Double free or corrupted node (bogus next pointer)", 
				K_DOUBLE_FREE, n);
			return;
		} else if(prev == BOGUS_POINTER) {
			k_error("Double free or corrupted node (bogus next pointer)", 
				K_DOUBLE_FREE, n);
			return;
		} 
		if(a->alloced == n)
			a->alloced = next;
		if(prev)
			prev->alloc_next = next;
		if(next)
			next->alloc_prev = prev;
		n->alloc_next = n->alloc_prev = BOGUS_POINTER;

		if(--a->nalloced == 0)
			demand(a->alloced == 0, bogus alloced);
#	endif

	enqueue(a, n, file, line);

#	ifdef DDEBUG
		demand(n->free, bogus free value);
#	endif
}

/*
 * Compute Internet Checksum for "count" bytes
 *         beginning at location "addr".
 * Taken from RFC 1071.
 */
static unsigned short 
inet_cksum(unsigned short *addr, unsigned short count) 
{
       register long sum = 0;

        /*  This is the inner loop */
        while( count > 1 )  {
               sum += * (unsigned short *) addr++;
               count -= 2;
        }

        /*  Add left-over byte, if any */
        if(count > 0)
                sum += * (unsigned char *) addr;
        /*  Fold 32-bit sum to 16 bits */
        while (sum>>16)
                sum = (sum & 0xffff) + (sum >> 16);
        return ~sum;
}


/* Allocate memory out of the default heap. */
static int 
default_grab_memory(void **p, int ignored)
{
	static int allocated;

	if(allocated) {
		*p = 0;
		return 0;
	} else {
		allocated = 1;
		*p = default_heap;
		return sizeof default_heap;
	}
}


  /*
	Do not use VAXCRTL's qsort() due to a severe bug:  once you've
	sorted something which has a size that's an exact multiple of 4
	and is longword aligned, you cannot safely sort anything which
	is either not a multiple of 4 in size or not longword aligned.
	A static "move-by-longword" optimization flag inside qsort() is
	never reset.  This is known of affect VMS V4.6 through VMS V5.5-1.

	In this work-around an insertion sort is used for simplicity.
	The qsort code from glibc should probably be used instead.
   */
static void
not_qsort (void *array, unsigned count, unsigned size, int (*compare)())
{

  if (size == sizeof (short))
    {
      register int i;
      register short *next, *prev;
      short tmp, *base = array;

      for (next = base, i = count - 1; i > 0; i--)
	{
	  prev = next++;
	  if ((*compare)(next, prev) < 0)
	    {
	      tmp = *next;
	      do  *(prev + 1) = *prev;
		while (--prev >= base ? (*compare)(&tmp, prev) < 0 : 0);
	      *(prev + 1) = tmp;
	    }
	}
    }
  else if (size == sizeof (long))
    {
      register int i;
      register long *next, *prev;
      long tmp, *base = array;

      for (next = base, i = count - 1; i > 0; i--)
	{
	  prev = next++;
	  if ((*compare)(next, prev) < 0)
	    {
	      tmp = *next;
	      do  *(prev + 1) = *prev;
		while (--prev >= base ? (*compare)(&tmp, prev) < 0 : 0);
	      *(prev + 1) = tmp;
	    }
	}
    }
  else  /* arbitrary size */
    {
      register int i;
      register char *next, *prev, *base = array;
      unsigned long tmp[size];
      for (next = base, i = count - 1; i > 0; i--)
	{   /* count-1 forward iterations */
	  prev = next,  next += size;		/* increment front pointer */
	  if ((*compare)(next, prev) < 0)
	    {	/* found element out of order; move others up then re-insert */
	      memcpy (tmp, next, size);		/* save smaller element */
	      do { memcpy (prev + size, prev, size); /* move larger elem. up */
		   prev -= size;		/* decrement back pointer */
		 } while (prev >= base ? (*compare)(tmp, prev) < 0 : 0);
	      memcpy (prev + size, tmp, size);	/* restore small element */
	    }
	}
    }

  return;
}
