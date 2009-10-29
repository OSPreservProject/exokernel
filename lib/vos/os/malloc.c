
/*
 * Copyright MIT 1999
 */

#include <stdio.h>

#include <xok/types.h>
#include <vos/vm-layout.h>
#include <vos/kprintf.h>
#include <vos/cap.h>
#include <vos/vm.h>


freelisthdr_t freelistarr[NUM_FLST];
static u_int memsizes[MAX_DMEM_PGS];
static u_int __dmemva;
static u_int __freedmemva;

#define MEMSIZE_NULL  0
#define MEMSIZE_ALLOC 1
#define MEMSIZEIDX(va) PGNO(va-DMEM_START)


#define dprintf if (0) kprintf


void
malloc_init(void)
{
  int i;
  for(i=0;i<NUM_FLST;i++)
    freelistarr[i].next = 0;

  for(i=0;i<MAX_DMEM_PGS;i++)
    memsizes[i] = MEMSIZE_NULL;

  __dmemva = DMEM_START;
  __freedmemva = 0;
}


static void
freelistdump()
{
  int i;
  for(i=0;i<NUM_FLST;i++)
  {
    u_int val = freelistarr[i].next;
    printf("freelist %d: ",i);
    while(val != 0)
    {
      printf("0x%x ",val);
      val = *((u_int*)val);
    }
    printf("\n");
  }
}


static inline int
find_freepgs(int num, u_int* va)
{
  int c=0;
  u_int start_va, last_va, last_sva;
  u_int iter = __freedmemva;

  start_va = __freedmemva;
  last_sva = 0;
  last_va = __freedmemva-NBPG;
  c = 0;
  while(iter != 0)
  {
    if (iter == last_va+NBPG)
    {
      c++;
      last_va = iter;
    } 
    
    else
    {
      last_sva = last_va;
      start_va = iter;
      last_va = iter;
      c = 1;
    }

    if (c == num) /* found a match */
    {
      /* remove from freelist */
      if (last_sva == 0)
	__freedmemva = memsizes[MEMSIZEIDX(iter)];
      else
	memsizes[MEMSIZEIDX(last_sva)] = memsizes[MEMSIZEIDX(iter)];

      *va = start_va;
      return c;
    }

    iter = memsizes[MEMSIZEIDX(iter)];
  }

  return -1;
}


void* 
imalloc(u_int size)
{
  int   pgsize = PGROUNDUP(size);
  int   ret,i;
  u_int bufva;

  if (__dmemva+pgsize > DMEM_START+MAX_DMEM_PGS*NBPG)
  {
    dprintf ("imalloc: exceeded dmem limit 0x%x\n",MAX_DMEM_PGS*NBPG);
    return 0L;
  }

  if ((ret = find_freepgs(PGNO(pgsize),&bufva)) <= 0)
  {
    /* ask kernel for a page */
    if ((ret = vm_alloc_region(__dmemva,pgsize,CAP_USER, PG_U|PG_W|PG_P)) < 0) 
    { 
      dprintf ("imalloc: vm_alloc_region failed %d\n",ret);
      return 0L;
    }

    /* for simplicity, we free all retured pages if we didn't get enough mem */
    else if (ret != PGNO(pgsize))
    {
      dprintf("imalloc: requested %d, got %d pages\n",pgsize,ret);
      if ((ret = vm_free_region(__dmemva,ret*NBPG,CAP_USER)) < 0) 
        dprintf("imalloc: recover failed, totally screwed\n");
      return 0L;
    }
    bufva = __dmemva;
    __dmemva += pgsize;
  }

  /* update memsizes */
  if (size <= (NBPG>>1))
    memsizes[MEMSIZEIDX(bufva)] = 1 << (NDX(size)+5);
  else
  {
    memsizes[MEMSIZEIDX(bufva)] = pgsize;
    for(i=1; i<ret; i++)
      memsizes[MEMSIZEIDX(bufva)+i] = MEMSIZE_ALLOC;
  }

  /* divide page up into chunks, store chunks (except for the first one) into
   * freelist */
  if (size < NBPG)
  {
    u_short ndx = NDX(size);
    u_short chunk = 1 << (NDX(size)+5);
    u_short ncpg = NBPG >> (NDX(size)+5);

    freelistarr[ndx].next = bufva+chunk;
    for(i = 1; i < ncpg-1; i++)
      *((u_int*)(bufva+chunk*i)) = bufva+chunk*(i+1);
    *((u_int*)(bufva+chunk*i)) = 0;
  }

  /* increment mem counter and return the first chunk */
  return (void*) bufva;
}


void
free(void* ptr)
{
  int idx = MEMSIZEIDX(ptr);
  u_int sz;
  u_int va = (u_int) ptr;

  if (idx < 0 || idx >= MAX_DMEM_PGS)
  {
    printf("free: can't free %p: exceeded dmem limit???\n",ptr);
    return;
  }
  
  sz = memsizes[idx];

  if (sz == MEMSIZE_ALLOC || sz == MEMSIZE_NULL || sz >= DMEM_START)
  {
    printf("free: can't free %p: not at boundary or null\n",ptr);
    return;
  }

  bzero(ptr,sz);

  if (sz <= (NBPG>>1))  /* chunks */
  {
    if (va & (sz-1) != 0)
      printf("free: pointer %p: not at size boundary of %d\n",ptr,sz); 
    else
    {
      /* add to freelist */
      u_short ndx = NDX(sz);
      *((u_int*)va) = freelistarr[ndx].next;
      freelistarr[ndx].next = va;
    }
  }

  else /* pages */
  {
    u_int pgs = PGNO(sz);
    int i=0;

    for(i=0; i<pgs-1; i++)
      memsizes[MEMSIZEIDX(va+i*NBPG)] = va+(i+1)*NBPG;
    memsizes[MEMSIZEIDX(va+i*NBPG)] = __freedmemva;
    __freedmemva = va;
  }
}


#define my_min(a,b) ((a)<(b))?(a):(b)

void*
realloc(void* ptr, u_int size)
{
  void *newptr;
  int idx = MEMSIZEIDX(ptr);
  u_int sz; 

  if (idx < 0 || idx >= MAX_DMEM_PGS)
  {
    printf("realloc: %p: exceeded dmem limit???\n",ptr);
    return NULL;
  }
  sz = memsizes[idx];

  if (sz == MEMSIZE_ALLOC || sz == MEMSIZE_NULL || sz >= DMEM_START)
  {
    printf("realloc: %p: not at boundary or null\n",ptr);
    return NULL;
  }

  if (size == 0)
  {
    free(ptr);
    return NULL;
  }

  newptr = malloc(size);

  if (ptr == NULL || newptr == NULL)
    return newptr;

  memmove(newptr, ptr, my_min(size,sz));
  free(ptr);

  return newptr;
}

