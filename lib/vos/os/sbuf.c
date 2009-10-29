
/*
 * Copyright MIT 1999
 */

#include <stdio.h>

#include <xok/types.h>
#include <xok/mmu.h>

#include <vos/errno.h>
#include <vos/proc.h> 
#include <vos/vm.h>
#include <vos/sbuf.h>
#include <vos/assert.h>


#define dprintf if (0) kprintf
#define DUMP_S(s) dprintf("sbuf: 0x%x, size 0x%x\n", (s).va, (s).size)


static u_int __sbufva;
static sbuf_pool_t *sbuf_pools = NULL;



/*
 * inverted ptable: physical to virtual address mapping for the sbuf region.
 */
typedef struct pvmap
{
  u_int ppn;
  u_int va;
  u_short refcnt;
  struct pvmap *next;
} pvmap_t;

static pvmap_t *pvmap_registry = NULL;


static inline int
sbuf_pvmap_insert(u_int ppn, u_int va)
{
  pvmap_t *p = pvmap_registry;

  assert(va >= SBUF_WINDOW_START && va < SBUF_WINDOW_END);

  while(p != NULL)
  {
    if ((p->va == va && p->ppn != ppn) ||
	(p->va != va && p->ppn == ppn))
    {
      printf("inconsistent pvmap entry: va %d %d, ppn %d %d\n", 
	  p->va,va,p->ppn,ppn);
      assert(0);
    }

    if (p->ppn == ppn && p->va == va)
    {
      p->refcnt++;
      return p->refcnt;
    }
    
    p = p->next;
  }

  p = (pvmap_t*) malloc(sizeof(pvmap_t));
  p->ppn = ppn;
  p->va = va;
  p->refcnt = 1;
  p->next = pvmap_registry;
  pvmap_registry = p;
  return 1;
}


int 
sbuf_find_mapping(u_int ppn)
{
  pvmap_t *p = pvmap_registry;

  while (p != NULL)
  {
    if (p->ppn == ppn)
      return p->va;
    p = p->next;
  }
  RETERR(V_INVALID);
}


static inline int
sbuf_pvmap_remove(u_int ppn)
{
  pvmap_t *prev = NULL;
  pvmap_t *p = pvmap_registry;

  while (p != NULL)
  {
    if (p->ppn == ppn)
    {
      p->refcnt--;
      if (p->refcnt == 0)
      {
	prev->next = p->next;
	free(p);
      }
      return 0;
    }
    prev = p;
    p = p->next;
  }
  return -1;
}



void
sbuf_pools_dump()
{
  sbuf_pool_t *pool = sbuf_pools;

  while(pool != NULL)
  {
    sbuf_t *s = pool->sbufs;
    printf("pool %p: k %d 0x%x\n",pool,pool->capability,pool->prot_flag);
    
    while(s != NULL)
    {
      printf("  * sbuf %p: va 0x%x, size 0x%x\n", s, s->va, s->size);
      s = s->next;
    }
    pool = pool->next;
  }
}


void
sbuf_init (void)
{
  __sbufva = SBUF_WINDOW_START;
  pvmap_registry = NULL;
  sbuf_pools = NULL;
}


/* constructs a sbuf pool */
static inline sbuf_pool_t* 
sbuf_pool_create(u_short k, u_int prot_flag)
{
  sbuf_pool_t *p = (sbuf_pool_t *) malloc(sizeof(sbuf_pool_t));
  
  if (p == NULL) 
    return p;

  p->capability = k;
  p->prot_flag = prot_flag;
  p->sbufs = NULL;

  p->next = sbuf_pools;
  sbuf_pools = p;

  return p;
}


/* returns the sbuf_pool_t instance with capability k */
static inline sbuf_pool_t* 
sbuf_pool_find(int k, u_int prot_flag)
{
  sbuf_pool_t *pool = sbuf_pools;

  while(pool != NULL)
  {
    if (pool->capability == k && pool->prot_flag == prot_flag) 
      return pool;
    pool = pool->next;
  }

  return NULL;
}


static inline int
sbuf_pool_insert_sbuf(sbuf_t* b)
{
  u_short k = b->capability;
  sbuf_pool_t *pool = sbuf_pool_find(k, va2pte(b->va)&PGMASK);
  sbuf_t *s;

  if (pool == NULL)
    pool = sbuf_pool_create(k, va2pte(b->va)&PGMASK);

  /* first, check to see if we can coalese */
  s = pool->sbufs;
  while(s != NULL)
  {
    if (s->va + s->size == b->va)
    {
      s->size += b->size;
      return 0;
    }
    
    else if (b->va + b->size == s->va)
    {
      s->va = b->va;
      s->size += b->size;
      return 0;
    }

    s = s->next;
  }

  /* can't coalese... */
  s = (sbuf_t*) malloc(sizeof(sbuf_t)); 
  
  if (s == NULL)
    RETERR (V_NOMEM);

  *s = *b;
  s->next = pool->sbufs;
  pool->sbufs = s;

  return 0;
}


static inline int
sbuf_create_sbuf(u_short k, size_t size, sbuf_t *b, 
                 sbuf_pool_t *pool, u_int flag)
{
  int ret;

  /* get pages from kernel */
  if ((ret = vm_alloc_region(__sbufva, NBPG, k, flag)) < 0)
  {
    dprintf ("sbuf_create_sbuf: vm_alloc_region failed %d\n",ret);
    RETERR(V_NOMEM);
  }
  
  else if (ret != 1)
  {
    dprintf("sbuf_create_sbuf: got %d pages (requested 1 page)\n",ret);
    RETERR(V_NOMEM);
  }

  __sbufva += NBPG;

  b->owner = getpid();
  b->va = __sbufva-NBPG;
  b->ppn = va2ppn(b->va);
  b->size = size;
  b->capability = k;
  b->next = NULL;

  sbuf_pvmap_insert(b->ppn, b->va);
  
  if (size < 4096)
  {
    sbuf_t *newb = (sbuf_t*) malloc(sizeof(sbuf_t)); 
    *newb = *b;
    newb->va = b->va+size;
    newb->size = NBPG - size;
    newb->next = pool->sbufs;
    pool->sbufs = newb;
  }

  return 0;
}


#define P2SIZE(size) \
  ((size) > 512 \
    ? (size) > 1024 \
      ? (size) > 2048 ? 4096 : 2048 \
      : 1024 \
    : (size) > 128  \
      ? (size) > 256 ? 512 : 256 \
      : (size) > 64 \
        ? 128 \
	: (size) > 32 ? 64 : 32)


#define dump_sbuf(s) printf("sbuf: 0x%8x, size 0x%8x\n", s->va, s->size)

int
sbuf_alloc(u_short k, size_t size, sbuf_t *b, u_int flag)
{
  sbuf_pool_t *pool;
  
  if (size > NBPG)
    RETERR(V_SBUF_TOOBIG);

  if (b == 0L)
    RETERR(V_INVALID);
  
  size = P2SIZE(size); /* always use power of 2 sizes */
  flag |= PG_U | PG_P;
  flag &= PGMASK;
  pool = sbuf_pool_find(k, flag);
  
  if (pool == NULL) /* no pool exists, create new sbuf */
  {
    pool = sbuf_pool_create(k, flag);
    return sbuf_create_sbuf(k, size, b, pool, flag);
  }

  else /* there is a pool */
  {
    u_char found = 0;
    sbuf_t *s = pool->sbufs;
    sbuf_t *p = NULL;
    
    dprintf("sbuf_alloc: found a pool %d, attempting to get %d bytes\n",k,size);

    /* first, look for perfect match */
    while(s != NULL && found == 0)
    {
      if (s->size == size)
      {
        dprintf("sbuf_alloc: found perfect match %p\n",s);
	DUMP_S(*s);
	found = 1;
	break;
      }
      p = s;
      s = s->next;
    }
  
    if (found == 0)
    {
      dprintf("sbuf_alloc: no perfect match\n");
      s = pool->sbufs;
      p = NULL;
    }

    /* next, look for anything */
    while(s != NULL && found == 0)
    {
      if (s->size > size)
      {
	found = 1;
	break;
      }
      p = s;
      s = s->next;
    }

    /* found a sbuf */
    if (found == 1)
    {
      dprintf("sbuf_alloc: found some match %p %p %d %d\n",b,s,s->size,size);
      *b = *s;

      /* perfect sbuf, remove existing record */
      if (s->size == size)
      {
	if (p != NULL)
	  p->next = s->next;
	else
	  pool->sbufs = NULL;
	free(s);
      }

      /* not perfect, shrink existing sbuf */
      else
      {
	assert(s->size > size);

  	b->size = size;
  	b->next = NULL;

  	s->va = s->va + size;
  	s->size = s->size - size;
      }

      b->owner = getpid();
      b->next = NULL;
  
      dprintf("returning: ");
      DUMP_S(*b);
      return 0;
    }

    /* got to create a new one */
    else
      return sbuf_create_sbuf(k, size, b, pool, flag);
  }

  assert(0);
  return -1;
}


int
sbuf_free (sbuf_t *b)
{
  if (b == 0L)
    RETERR (V_INVALID);

  if (b->size == 0)
    return 0;

  assert(b->size <= NBPG);

#if 0
  if (b->owner == getpid())
#endif
  {
    if (sbuf_pool_insert_sbuf(b) < 0)
      return -1;
  }
#if 0
  else
    printf("sbuf_free for %d: not owner (%d)!\n",getpid(),b->owner);
#endif

  b->owner = 0;
  b->size = 0;
  b->va = 0;
  b->next = NULL;
  return 0;
}


int
sbuf_map_page(int k, Pte *pte)
{
  if (((*pte)>>PGSHIFT) == 0)
  {
    int r;
    r = vm_alloc_region(__sbufva, NBPG, k, (*pte)&PGMASK);
    if (r < 0)
      RETERR(V_NOMEM);
    *pte = va2pte(__sbufva);
  }
  else
  {
    if (vm_self_insert_pte(k, *pte, __sbufva, 0, 0) < 0)
      RETERR(V_CANTMAP);
  }
  
  sbuf_pvmap_insert(va2ppn(__sbufva),__sbufva);
  __sbufva += NBPG;
  return __sbufva-NBPG;
}


