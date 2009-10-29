
#ifndef __VOS_SYNC_H__
#define __VOS_SYNC_H__

#include <vos/cap.h>
#include <vos/sbuf.h>
#include <vos/locks.h>

/*
 * below is a set of macros that define a shared data structure used in our
 * synchronization algorithm.
 */

#define MK_SHARED_RECORD(t,sstr,pstr) \
typedef sstr t##_pri_t; \
typedef pstr t##_shr_t; \
typedef struct \
{ \
  unsigned int latest; \
  spinlock_t lock; \
  sbuf_t where; \
  t##_shr_t master; \
} t##_meta_t; \
typedef struct { \
  t##_pri_t pri; \
  sbuf_t __self; /* COW */ \
  t##_shr_t *self; \
  u_int s_size; \
  sbuf_t __meta; /* SHARED */ \
  t##_meta_t *meta; \
} t##_srec_t;

#define INIT_SHARED_RECORD(t,v,ks,kp) \
{ \
  sbuf_alloc(ks, sizeof(t##_shr_t), &((v).__self),PG_W); \
  (v).self = SBUF((v).__self,t##_shr_t*); \
  (v).s_size = sizeof(t##_shr_t); \
  sbuf_alloc(kp, sizeof(t##_meta_t), &((v).__meta),PG_W|PG_SHARED); \
  (v).meta = SBUF((v).__meta,t##_meta_t*); \
  ((v).meta)->latest = 0; \
  spinlock_reset(&((v).meta)->lock); \
}


#define SR_T(t) t##_srec_t  /* shared record type */
#define S_T(t) t##_shr_t    /* shared data type */

#if 1

#define SR_LOCK(srptr) \
  spinlock_acquire(&(srptr)->meta->lock);
#define SR_UNLOCK(srptr) \
  spinlock_release(&(srptr)->meta->lock);

#else

#define SR_LOCK(srptr) cpulock_acquire();
#define SR_UNLOCK(srptr) cpulock_release();

#endif


#define SR_UPDATE_META(srptr) \
{ \
  (srptr)->meta->latest = getpid(); \
  (srptr)->__self.ppn = va2ppn((u_int)((srptr)->self)); \
  (srptr)->__self.owner = getpid(); \
  (srptr)->meta->where = (srptr)->__self; \
}

#define SR_MOVETO_MASTER(srptr) \
{ \
  (srptr)->meta->latest = 1; \
  (srptr)->meta->master = *(srptr)->self; \
  memmove((char*)&((srptr)->meta->master), \
          (char*)((srptr)->self), (srptr)->s_size); \
}
  

extern int sr_localize;
#define SR_LOCALIZE(srptr,verify,bail) \
{\
  if ((srptr)->meta->latest != getpid() && \
      (srptr)->meta->latest != 0 && \
      (srptr)->meta->latest != 1) \
  { \
    sbuf_t b; \
    int r; \
    sr_localize++; \
    b = ((srptr)->meta->where); \
    r = sbuf_find_mapping(b.ppn); \
    if (r < 0) \
    { \
      Pte pte = (b.ppn << PGSHIFT) | PG_U | PG_P; \
      r = sbuf_map_page(CAP_USER, &pte); \
    } \
    memmove((char*)((srptr)->self), \
	    (char*)(r+SBUF_PTE_OFFSET(&b)),(srptr)->s_size); \
  } \
  else if ((srptr)->meta->latest == 1) \
  { \
    memmove((char*)((srptr)->self), \
            (char*)&((srptr)->meta->master), (srptr)->s_size); \
  } \
  if (verify((srptr)->self) < 0) \
    bail; \
}

#define SR_DUMP_META(srptr) \
{ \
  printf("env %d: meta data at %d: latest: %d, sbuf: %d, self: %d\n", \
      getpid(), va2ppn((u_int)(srptr)->meta), \
      (srptr)->meta->latest,(srptr)->meta->where.ppn,(srptr)->__self.ppn); \
}



#endif /* __VOS_SBUF_H__ */


