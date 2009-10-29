
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

/* XXX checked - tom */



#define __ENV_MODULE__

#include <xok/defs.h>
#include <dpf/dpf.h>
#include <xok/mmu.h>
#include <xok/env.h>
#include <xok/pmap.h>
#include <xok/sysinfo.h>
#include <xok/capability.h>
#include <xok/syscall.h>
#include <xok/sys_proto.h>
#include <dpf/dpf-internal.h>
#include <xok/wk.h>
#include <xok/ash.h>
#include <xok_include/assert.h>
#include <xok/kstrerror.h>
#include <xok/scheduler.h>
#include <xok/locore.h>
#include <xok/init.h>
#include <xok/printf.h>
#include <xok/malloc.h>
#include <xok/ipc.h>

#define envs __envs

extern Pte *ppage_upt;  /* Page table for read-only ppage structures */
extern Pte *envsi_upt;  /* Page table for r/o sysinfo and env structures */

/* initial BSP p0cr3 and p0pdir, saved for pirating later */
unsigned int p0cr3;
unsigned int *p0pdir;

struct Env *envs;			/* All environments */

/* the init process */
struct Env* init_env;

static u_long next_env_id;		/* Upper bits of Unique identifier */
static struct Env_list env_free_list;	/* free list */
static struct Env_list env_used_list;	/* not free list */

static struct Ts p0ts[NR_CPUS];

#ifdef ASH_ENABLE
static inline void
mapash (int eid, u_int pde)
{
  struct Env *e;
  int ashno = PDENO (ASHVM) + envidx (eid);

  for (e = env_used_list.lh_first; e; e = e->env_link.le_next)
  {
    int i;
    for (i=0; i<NR_CPUS; i++)
    {
      if (e->env_pd->envpd_cr3[i] != 0)
        e->env_pd->envpd_pdir[i][ashno] = pde;
    }
  }
}
#endif /* ASH_ENABLE */


void 
load_idle_env(struct Env* e, int cpu)
{
  int i, r;
  u_int va;
  struct Ppage *pp;
  extern u_char env0_bin[];
  extern const int env0_bin_size;

  /* Write the program into memory. We start the program offset into
     the first page by 0x20 bytes as if the a.out header were at the
     start of the image. We do this since our binaries are compiled to
     run at UTEXT+0x20 so that the binary can be mmapped directly from
     disk, header and all. */

  va = UTEXT;
  for (i = 0; i < env0_bin_size;)
    {
      if ((r = ppage_alloc (PP_USER, &pp, 0)) < 0) 
	panic ("env_init: could not alloc page for env 0. Errno %d: %s\n",
	       r, kstrerror(r));
      if (i == 0)
	{
	  bcopy (&env0_bin[i], pp2va (pp) + 0x20, NBPG - 0x20);
	  i += (NBPG - 0x20);
	}
      else
	{
	  bcopy (&env0_bin[i], pp2va (pp), NBPG);
	  i += NBPG;
	}
      if ((r = ppage_insert 
	    (&envs[envidx(e->env_id)], pp, va, PG_P | PG_W | PG_U)) < 0)
	{
	  panic ("env_init: could not map page for env 0. Errno %d: %s\n",
		 r, kstrerror(r));
	}
      va += NBPG;
    }


  /* Give it a stack */
  for (va = USTACKTOP - USTKSIZE; va < USTACKTOP; va += NBPG)
    {
      if ((r = ppage_alloc (PP_USER, &pp, 0)) < 0) 
	panic ("env_init: could not alloc page for env 0's stack. "
	       "Errno %d: %s\n", r, kstrerror(r));
      bzero (pp2va (pp), NBPG);
      if ((r = ppage_insert (e, pp, va, PG_P | PG_W | PG_U)) < 0)
	{
	  panic ("env_init: could not map stack page for env 0. "
		 "Errno %d: %s\n", r, kstrerror(r));
	}
    }

  /* Put us in the current environment (for fake syscalls) */
  __cpucxts[cpu]->_current_env = e;
#ifdef __SMP__
  __cpucxts[cpu]->_current_env->env_cur_cpu = cpu;
#endif

  e->env_tf.tf_eip = UTEXT + 0x20;
  /* Copy the fake trapframe to the top of the stack for env_run */
  bcopy (&envs[envidx(e->env_id)].env_tf, utf, sizeof (struct Trapframe));
  /* Give env0 the zero length capability with all perms */
  cap_init_zerolength (&e->env_clist[0]);

  /* Setup the U structure (which should already be mapped */
  if (!envs[envidx(e->env_id)].env_u)
    panic ("env_init: no U structure in env 0\n");
  bzero (envs[envidx(e->env_id)].env_u, sizeof (*envs[envidx(e->env_id)].env_u));
  envs[envidx(e->env_id)].env_u->u_status = U_RUN;
  envs[envidx(e->env_id)].env_u->u_donate = -1;
  envs[envidx(e->env_id)].env_u->u_revoked_pages = 0;

  /* Initially the process is not inside a protmeth (go figure!) */
#ifdef __PAM__
  e->prot_abs_id = -1;
#endif
  
  __cpucxts[cpu]->_idle_env = e;
}


#ifdef __SMP__

#include <xok/cpu.h>

void
env_init_secondary (void)
  /*
   * - setup new kernel stack
   * - load a real page table
   * - initialize TSS for secondary processors 
   */
{
  int cpu = get_cpu_id ();
  struct Env *e;
  int gdt_selector;
  int r;
  int j, jj;
  Pte *pte;

  /* Define the u-dot structure */
  DEF_SYM (UAREA, UADDR);

  /* Allocate the idle loop (environment 0). */
  if ((r = env_alloc (&e, 1, cpu)) < 0)
    {
      panic ("env_init_secondary: could not allocate env for idle proc. "
	     "Errno %d: %s\n", r, kstrerror(r));
    }
  
  /* ppage_init_secondary has already created a pt for kernel stack, but
   * that's at the bottom of the directory VA, we want to get kernel stack to
   * the top */
  
  jj = j = PTENO(KSTACKTOP-KSTKSIZE);
  for (; j < NLPG; j++)
    kstack_pts[cpu][j] = kstack_pts[cpu][j - jj];

  pte = (Pte *)ptov(e->env_pd->envpd_pdir[cpu][PDENO(KSTACKTOP - 1)]&~PGMASK);
  for (j = 0; j < PTENO (KSTACKTOP - KSTKSIZE); j++)
    kstack_pts[cpu][j] = pte[j];

  e->env_pd->envpd_pdir[cpu][PDENO(KSTACKTOP-1)] =
    kva2pa(kstack_pts[cpu])|PG_W|PG_P;

  /* move over the new cr3 */
  asm volatile ("movl %0,%%cr3" :: "r" (e->env_pd->envpd_cr3[cpu]));

  /* 
   * Setup a TSS (primarily so that we get the right stack when we
   * trap to the kernel). Also, the TR selector is how we determine 
   * which CPU we are on later in kernel. 
   */
  p0ts[cpu].ts_esp0 = KSTACKTOP;
  p0ts[cpu].ts_ss0 = GD_KD;
  p0ts[cpu].ts_cr3 = e->env_pd->envpd_cr3[cpu];
  p0ts[cpu].ts_iomb = 0xdfff;
  gdt_selector = (GD_TSS >> 3) + cpu;
  settss (gdt[gdt_selector], &(p0ts[cpu]));
  asm volatile ("ltr %0"::"r" (gdt_selector << 3));
  
  load_idle_env(e, cpu);
  
  printf ("Processor %d: tss %d, ", cpu, gdt_selector);
  printf ("stack at 0x%x, ", e->env_pd->envpd_cr3[cpu]);
  printf ("idle env %d\n", e->env_id);
}

#endif



void
env_init (void)
{
  int i, r;
  int cpu = get_cpu_id ();
  u_int pa;
  struct Env *e;
  Pte *ptep;

  if (cpu != 0)
    panic ("Non-BSP running env_init... VERY BAD!!!");

  /* Define the u-dot structure */
  DEF_SYM (UAREA, UADDR);

  /* Initialize lists */
  LIST_INIT (&env_free_list);
  LIST_INIT (&env_used_list);

  StaticAssert(KSTKSIZE <= KSTACKGAP);
  StaticAssert(sizeof (struct Uenv) <= NBPG);
  StaticAssert(NENV*NBPG <= UADDRS_SIZE);
  StaticAssert(PTENO(UADDR) == 0);
#ifdef ASH_ENABLE
  StaticAssert (NENV <= NASHPD);
#endif

  /* set our page table addr */
  p0cr3 = p0cr3_boot;
  p0pdir = p0pdir_boot;

  /* Stick the environments on the free list */
  for (i = NENV - 1; i >= 0; i--)
    LIST_INSERT_HEAD (&env_free_list, &envs[i], env_link);

  /* Allocate the idle loop (environment 0). */
  if ((r = env_alloc (&e, 1, 0)) < 0)
    {
      panic ("env_init: could not allocate env for idle proc. "
	     "Errno %d: %s\n", r, kstrerror(r));
    }

  /* save CPU 0's kernel stack PDE */
  kstack_pts[0] = 
    (Pte *)ptov(e->env_pd->envpd_pdir[0][PDENO(KSTACKTOP-1)]&~PGMASK);

  p0pdir = e->env_pd->envpd_pdir[0];
  asm volatile ("movl %0,%%cr3" :: "r" (e->env_pd->envpd_cr3[0]));
  ppage_free (pa2pp (p0cr3));
  p0cr3 = e->env_pd->envpd_cr3[0];

  /* 
   * Setup a TSS (primarily so that we get the right stack when we
   * trap to the kernel). Also, the TR selector is how we determine 
   * which CPU we are on later in kernel. 
   */
  p0ts[0].ts_esp0 = KSTACKTOP;
  p0ts[0].ts_ss0 = GD_KD;
  p0ts[0].ts_cr3 = p0cr3; /* this is not used at all */
  p0ts[0].ts_iomb = 0xdfff;
  settss (gdt[GD_TSS >> 3], &(p0ts[0]));
  asm volatile ("ltr %0" :: "r" (GD_TSS));
  asm volatile ("lldt %0" :: "m" (0));

  load_idle_env(e, 0);

  for (ptep = (Pte *) ptov (kstkmap) + PTENO (ASHDVM + IOPHYSMEM),
	 pa = IOPHYSMEM | PG_P | PG_U | PG_W;
       pa < EXTPHYSMEM; pa += NBPG, ptep++)
    *ptep = pa;

  SYSINFO_ASSIGN(si_killed_envs,0);
  
  /* allocate the init env (environment 1). */
  if ((r = env_alloc (&init_env, 1, 0)) < 0)
    {
      panic ("env_init: could not allocate env for idle proc. "
	     "Errno %d: %s\n", r, kstrerror(r));
    }
  
  printf ("Processor 0: tss %d, ", GD_TSS >> 3);
  printf ("stack at 0x%x, ", p0ts[0].ts_esp0);
  printf ("idle env %d\n", e->env_id);
}



int
env_dup_pdir_forcpu(struct Env *e)
  __XOK_SYNC(e->env_pd->envpd_spinlock)
{
  extern struct Ppage* pd_duplicate(Pde *const);
  struct Ppage *pp;
  int r;
  
  assert(e->env_pd->envpd_cr3[cpu_id] == 0);
  
  MP_SPINLOCK_GET(&e->env_pd->envpd_spinlock);

  pp = pd_duplicate(e->env_pd->envpd_pdir[e->env_pd->envpd_active]);
  e->env_pd->envpd_cr3[cpu_id] = pp2pa(pp);
  e->env_pd->envpd_pdir[cpu_id] = pp2va(pp);

  /* remove pt at UADDR */
  e->env_pd->envpd_pdir[cpu_id][PDENO(UADDR)] = 0;

  /* allocate a new pt at UADDR */
  if ((r = pde_check_n_alloc_one(e->env_pd->envpd_pdir[cpu_id], UADDR)) < 0)
  {
    ppage_free(pp);
    e->env_pd->envpd_cr3[cpu_id] = 0;
    e->env_pd->envpd_pdir[cpu_id] = 0L;
    
    MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
    return -1;
  }

  /* map in our CPUContext structure and kernel stack */
  e->env_pd->envpd_pdir[cpu_id][PDENO(CPUCXT)]
    =kva2pa(cpucxt_pts[cpu_id])|PG_W|PG_P;
  e->env_pd->envpd_pdir[cpu_id][PDENO(KSTACKTOP-1)]
    =kva2pa(kstack_pts[cpu_id])|PG_W|PG_P;

  MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
  return 0;
}



int 
env_clone(struct Env **new)
{
  struct Env *e;
  struct Ppage *pp;
  int r;

  assert(curenv != 0L);

  if ((r = env_alloc(new, 0, 0)) < 0) 
    return r;

  e = *new;
#ifdef __SMP__
  assert(env_localize(e) == 0);
#endif

  e->env_pd = curenv->env_pd;
 
  MP_SPINLOCK_GET(&e->env_pd->envpd_spinlock);

  e->env_pd->envpd_rc++;

  /* Allocate a U structure */
  if ((r = ppage_alloc (PP_USER, &pp, 0)) < 0)
  {
    MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
    kill_env(e);
    return r;
  }

  if ((r = ppage_insert 
	(e, pp, UADDRS+envidx(e->env_id)*NBPG, PG_P | PG_W | PG_U)) < 0)
  {
    MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
    kill_env(e);
    return r;
  }

  e->env_u = pp2va(pp);
  e->env_upa = pp2pa(pp);

  MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
  e->env_status = ENV_OK;
 
#ifdef __SMP__
  env_release(e);
#endif
  return r;
}



int
env_alloc(struct Env **new, int newpt, int cpu) 
  __XOK_SYNC(locks ENV_LIST_LOCK;
             releases ENV_LIST_LOCK;
	     calls ppage_alloc; calls ppage_free;
	     locks ENV_LIST_LOCK;
	     releases ENV_LIST_LOCK)
{
  struct Env *e;
  struct Ppage *pp, *pp2;
  int i, r;
  extern Pte *bc_upt;		/* declared in bc.c */
  extern Pte *xnmap_upt;	/* declared in pmap.c */

  StaticAssert (sizeof (struct Uenv) <= NBPG);

  MP_QUEUELOCK_GET(GQLOCK(ENV_LIST_LOCK));

  if (!(e = env_free_list.lh_first)) {
    MP_QUEUELOCK_RELEASE(GQLOCK(ENV_LIST_LOCK));
    return -E_NO_FREE_ENV;
  }
  
  *new = e;
  LIST_REMOVE (e, env_link);
  bzero (e, sizeof (*e));
  LIST_INSERT_HEAD (&env_used_list, e, env_link);
  
  MP_QUEUELOCK_RELEASE(GQLOCK(ENV_LIST_LOCK));

  MP_SPINLOCK_INIT(&(e->env_spinlock));
  MP_RWLOCK_INIT(&(e->cap_rwlock));
    
  e->env_id = (next_env_id++ << (1 + LOG2NENV)) | (e - envs);
  e->env_clen = MAX_CAPNO;
  e->env_pred_pgs = NULL;

  /* now, build page table */
  if (newpt)
  {
    e->env_pd = (struct EnvPD*) malloc (sizeof(struct EnvPD));
    bzero(e->env_pd, sizeof(struct EnvPD));

    /* Allocate a page for the page directory */
    if ((r = ppage_alloc (PP_KERNEL, &pp, 0)) < 0) 
      goto env_alloc_err;
    
    for (i=0; i<NR_CPUS; i++)
    {
      e->env_pd->envpd_cr3[i] = 0;
      e->env_pd->envpd_pdir[i] = 0L;
    }

    e->env_pd->envpd_cr3[cpu] = pp2pa (pp);
    e->env_pd->envpd_pdir[cpu] = pp2va (pp);
    e->env_pd->envpd_active = cpu;
    e->env_pd->envpd_rc = 1;
    bzero (e->env_pd->envpd_pdir[cpu], NBPG);
    
    e->env_pd->envpd_nptes = malloc (NLPG * sizeof (e->env_pd->envpd_nptes[0]));
    if (!e->env_pd->envpd_nptes)
    {
      ppage_free(pp);
      free(e->env_pd);
      LIST_REMOVE (e, env_link);
      LIST_INSERT_HEAD (&env_free_list, e, env_link);
      r = -E_NO_MEM;
      goto env_alloc_err;
    }
    bzero (e->env_pd->envpd_nptes, NLPG * sizeof (e->env_pd->envpd_nptes[0]));
  
    /* Map kernel stack and physical memory */
    for (i = PDENO (KSTACKTOP - 1); i < NLPG; i++)
      e->env_pd->envpd_pdir[cpu][i] = p0pdir[i];

    /* Map in default CPUContext structure */
    e->env_pd->envpd_pdir[cpu][PDENO(CPUCXT)]
      = kva2pa(cpucxt_pts[cpu]) | PG_W | PG_P;
  
    /* Map vpt/vpd */
    e->env_pd->envpd_pdir[cpu][PDENO(VPT)]
      = e->env_pd->envpd_cr3[cpu] | PG_P | PG_W;
    e->env_pd->envpd_pdir[cpu][PDENO(UVPT)]
      = e->env_pd->envpd_cr3[cpu] | PG_P | PG_U;
  
    /* Map the data structures */
    e->env_pd->envpd_pdir[cpu][PDENO(UPPAGES)]
      = kva2pa(ppage_upt) | PG_U | PG_P;
    e->env_pd->envpd_pdir[cpu][PDENO(UENVS)]
      = kva2pa(envsi_upt) | PG_U | PG_P;
    e->env_pd->envpd_pdir[cpu][PDENO(UBC)]
      = kva2pa(bc_upt) | PG_U | PG_P;
    e->env_pd->envpd_pdir[cpu][PDENO(UXNMAP)]
      = kva2pa(xnmap_upt) | PG_U | PG_P;

#ifdef ASH_ENABLE
    /* Map the other ashs areas */
    for (i = PDENO (ASHVM); i < PDENO (ASHVM) + NASHPD; i++)
      e->env_pd->envpd_pdir[cpu][i] = p0pdir[i];

    /* Create a new ash area */
    if ((r = ppage_alloc (PP_KERNEL, &pp, 0) < 0))
    {
      free(e->env_pd->envpd_nptes);
      ppage_free(pp);
      free(e->env_pd);

      MP_QUEUELOCK_GET(GQLOCK(ENV_LIST_LOCK));
      LIST_REMOVE (e, env_link);
      LIST_INSERT_HEAD (&env_free_list, e, env_link);
      MP_QUEUELOCK_RELEASE(GQLOCK(ENV_LIST_LOCK));
     
      printf ("could not alloc page for ash area");
      goto env_alloc_err;
    }
    bzero (pp2va (pp), NBPG);
    /* Note PDENO (0) is really PDENO (e->env_ashuva) */
    e->env_pd->envpd_pdir[cpu][PDENO (0)] = pp2pa (pp) | PG_P | PG_W | PG_U;
    /* Map it globally */
    mapash (e->env_id, pp2pa (pp) | PG_P | PG_W);
#endif /* ASH_ENABLE */

    /* Allocate a U structure */
    if ((r = ppage_alloc (PP_USER, &pp2, 0)) < 0)
    {
      free(e->env_pd->envpd_nptes);
      ppage_free(pp);
      free(e->env_pd);
     
      MP_QUEUELOCK_GET(GQLOCK(ENV_LIST_LOCK));
      LIST_REMOVE (e, env_link);
      LIST_INSERT_HEAD (&env_free_list, e, env_link);
      MP_QUEUELOCK_RELEASE(GQLOCK(ENV_LIST_LOCK));
     
      goto env_alloc_err;
    }

    /* It is always mapped at UADDRS+envidx(e->env_idx)*NBPG */
    if ((r = ppage_insert
	  (e, pp2, UADDRS+envidx(e->env_id)*NBPG, PG_P | PG_W | PG_U)) < 0)
    {
      free(e->env_pd->envpd_nptes);
      ppage_free(pp);
      ppage_free(pp2);
      free(e->env_pd);
      
      MP_QUEUELOCK_GET(GQLOCK(ENV_LIST_LOCK));
      LIST_REMOVE (e, env_link);
      LIST_INSERT_HEAD (&env_free_list, e, env_link);
      MP_QUEUELOCK_RELEASE(GQLOCK(ENV_LIST_LOCK));
     
      goto env_alloc_err;
    }

    e->env_u = pp2va(pp2);
    e->env_upa = pp2pa(pp2);

    if ((r = pde_check_n_alloc_one(e->env_pd->envpd_pdir[cpu], UADDR)) < 0)
    {
      env_free(e);
      goto env_alloc_err;
    }

    ((Pte *) ptov(e->env_pd->envpd_pdir[cpu][PDENO(UADDR)] & ~PGMASK)) 
      [PTENO(UADDR)] = e->env_upa | PG_P | PG_W | PG_U;

    for (i=0; i<NR_CPUS; i++)
      e->env_pd->envpd_last_eid[i] = 0;

    MP_SPINLOCK_INIT(&(e->env_pd->envpd_spinlock));
    e->env_pd->envpd_id = e->env_id;
  }

#ifdef ASH_ENABLE
  e->env_ashuva = 0;
#endif

  /* Set up user stack in trapframe */
  e->env_tf.tf_ss = GD_UD | 3;
  e->env_tf.tf_esp = USTACKTOP;

  /* Fill in some other registers with useful values */
  e->env_tf.tf_ds = e->env_tf.tf_es = GD_UD | 3;
  e->env_tf.tf_cs = GD_UT | 3;

  e->env_tf.tf_eip = UTEXT;
  e->env_tf.tf_eflags = FL_IF;

  e->env_ticks = 0;
  e->env_ctxcnt = 0;

  /* Don't allow ipc's */
  e->env_allowipc1 = XOK_IPC_BLOCKED;
  e->env_allowipc2 = XOK_IPC_BLOCKED;
  
  e->msgring = 0L;

  /* Initially the process is not using any protected abstraction */
#ifdef __PAM__
  e->prot_abs_id = -1;
#endif

  /* Make it official at the very end */
#ifdef __SMP__
  e->env_cur_cpu = -1;
  e->env_last_cpu = -1;
#endif
  
  if (newpt) 
    e->env_status = ENV_OK;

  r = 0; /* should fall through */
env_alloc_err:
  return r;
}


void
env_free (struct Env *e) 
  __XOK_REQ_SYNC(e localized)
  __XOK_SYNC(locks e->env_spinlock;
             calls ppage_free;
	     locks ENV_LIST_LOCK)
{
  u_int pdeno;
  Pte *pt;
  struct Ppage *pp;
  u_int pteno;
  u_int npte;
  extern void msgring_free (msgringent * ringhead);

  MP_SPINLOCK_GET(&e->env_spinlock);
  MP_SPINLOCK_GET(&e->env_pd->envpd_spinlock);
  
  /* always remove our UAREA page */
  ppage_remove(e, UADDRS+envidx(e->env_id)*NBPG);

  e->env_pd->envpd_rc--;
     
  /* only remove page directory if we are the last one using it */
  if (e->env_pd->envpd_rc == 0)
  {
    int i;

    /* Flush all pages */
    for (pdeno = 0; pdeno < PDENO (ULIM); pdeno++)
    {
      /* skip pages that are not allocated per env */
      if (pdeno >= PDENO (UTOP) && pdeno < PDENO (UADDRS))
	continue;

      if (!(e->env_pd->envpd_pdir[e->env_pd->envpd_active][pdeno] & PG_P)) 
        continue;

      npte = e->env_pd->envpd_nptes[pdeno];
      if (npte > NLPG) 
      { 
        warn ("env_npte corrupted in env_free....assuming NLPG"); 
        npte = NLPG; 
      }

      pt = ptov(e->env_pd->envpd_pdir[e->env_pd->envpd_active][pdeno]&~PGMASK);
      for (pteno = 0; npte; pteno++) 
      { 
        if (pt[pteno] & PG_P) 
        { 
	  ppage_remove (e, (pdeno << PDSHIFT) | (pteno << PGSHIFT)); 
	  npte--; 
        } 
      }

      pp = kva2pp((u_long)pt);
      Ppage_pp_klock_acquire(pp);
      ppage_free (pp);
      Ppage_pp_klock_release(pp);
    }

    free (e->env_pd->envpd_nptes);
    
    for (i=0; i<NR_CPUS; i++)
    {
      if (e->env_pd->envpd_cr3[i] != 0)
      {
        if (e->env_pd->envpd_pdir[i][PDENO(UADDR)] & PG_P)
	{
          pt = ptov (e->env_pd->envpd_pdir[i][PDENO(UADDR)] & ~PGMASK);

          pp = kva2pp((u_long)pt);
          Ppage_pp_klock_acquire(pp);
          ppage_free (kva2pp ((u_long) pt));
          Ppage_pp_klock_release(pp);
	}
        pp = kva2pp ((u_long) (e->env_pd->envpd_pdir[i]));
        Ppage_pp_klock_acquire(pp);
        ppage_free (pp);
        Ppage_pp_klock_release(pp);
      }
    }

    MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);
    free(e->env_pd);
  }

  else
    MP_SPINLOCK_RELEASE(&e->env_pd->envpd_spinlock);

  /* punt any scheduling predicate */
  wk_free (e);
  
  /* punt references to any filters we have */
  dpf_del_env (e->env_id);

  /* punt message ring */
  msgring_free(e->msgring);
  e->msgring = 0L;

  /* increment the killed envs count */
  MP_QUEUELOCK_GET(GQLOCK(SYSINFO_LOCK));
  INC_FIELD(si,Sysinfo,si_killed_envs,1);
  MP_QUEUELOCK_RELEASE(GQLOCK(SYSINFO_LOCK));

  e->env_pd = 0L;
  e->env_status = ENV_FREE;

  e->env_cur_cpu = e->env_last_cpu = -1;

  MP_QUEUELOCK_GET(GQLOCK(ENV_LIST_LOCK));
  LIST_REMOVE (e, env_link);
  LIST_INSERT_HEAD (&env_free_list, e, env_link);
  MP_QUEUELOCK_RELEASE(GQLOCK(ENV_LIST_LOCK));

#ifdef ASH_ENABLE
  mapash (e->env_id, 0);
#endif

  MP_SPINLOCK_RELEASE(&e->env_spinlock);
}


struct Env *
env_access (u_int k, u_int envid, u_char perm, int *error) 
  __XOK_SYNC(calls env_getcap; locks e->env_spinlock; calls acl_access)
{
  struct Env *e;
  cap c;
  int r;

  assert(curenv != 0L);

  if ((r = env_getcap (curenv, k, &c)) < 0)
  {
    *error = r;
    return (NULL);
  }
  if (!(e = env_id2env (envid, error)))
    return (NULL);

  MP_RWLOCK_READ_GET(&e->cap_rwlock);
  
  if ((r = acl_access (&c, e->env_clist, 1, perm)) < 0)
  {
    *error = r;
    e = NULL;
  }

  MP_RWLOCK_READ_RELEASE(&e->cap_rwlock);

  return e;
}


void
kill_env (struct Env *e) 
  __XOK_REQ_SYNC(e localized)
  __XOK_SYNC(calls env_free)
{
  env_free (e);
  if (curenv == e)
    {
      curenv = NULL;
      sched_runnext ();
    }
}


/* protect the env's u-area with the give cap */
void
set_cap_uarea (struct Env *e, cap * c) 
  __XOK_REQ_SYNC(on e->env_pd->envpd_spinlock;  calls pp->pp_klock)
{
  Pte *ptep;
  struct Ppage *pp;

  ptep = pt_get_ptep (e->env_pd->envpd_pdir[e->env_pd->envpd_active], 
                      UADDRS+envidx(e->env_id)*NBPG);
  pp = ppages_get(*ptep >> PGSHIFT);
  ppage_setcap (pp, c);
}


/* do a stack backtrace */
void
backtrace (tfp tf)
{
  unsigned int ebp;
  unsigned int eip;
  int repeats;
  int foundend = 0;
  int m;

  printf ("***** starting stack bactrace *****\n");

#define BACKTRACE_MAXFRAMES	5

  ebp = tf->tf_ebp;
  repeats = BACKTRACE_MAXFRAMES;
  while (repeats > 0)
    {
      repeats--;

      /* this is kind of bogus, but it makes sure that pagefaults go to 
	 the application and not to us */
      m = page_fault_mode;
      page_fault_mode = PFM_PROP;

      if (ebp < 0x9000000)
	{
	  printf ("ack! bogus ebp = %x\n", ebp);
	  page_fault_mode = m;
	  return;
	}

      eip = *(unsigned *) (ebp + 4);
      ebp = *(unsigned *) ebp;
      page_fault_mode = m;

      eip -= 4;
      if (eip < 0x8000ff)
	{
	  foundend = 1;
	  break;
	}
      printf ("\tcalled from 0x%x\n", eip);
    }
  if (foundend)
    {
      printf ("***** traced back to start *****\n");
    }
  else
    {
      printf ("***** quit backtrace at %d levels *****\n", BACKTRACE_MAXFRAMES);
    }
}




/*
 *  System calls for maniuplating environments.
 */

DEF_ALIAS_FN (sys_env_clone, sys_env_alloc);
u_int
sys_env_alloc (u_int sn, u_int k, int *error) 
  __XOK_SYNC(calls env_alloc; locks e->env_pd->envpd_spinlock)
{
  struct Env *e;
  cap c;
  int r;

  if ((r = env_getcap (curenv, k, &c)) < 0)
  {
    copyout(&r, error, sizeof(int));
    return (0);
  }

  
  /* An environment must be seeded with a full capability */
  if ((c.c_perm & CL_ALL) != CL_ALL)
  {
    r = -E_CAP_INSUFF;
    copyout(&r, error, sizeof(int));
    return (0);
  }

  if (sn == SYS_env_alloc)
  {
    if ((r = env_alloc(&e, 1, 0)) < 0)
    {
      copyout(&r, error, sizeof(int));
      return (0);
    }
  } 
  else /* sn == SYS_env_clone */
  {
    if ((r = env_clone(&e)) < 0)
    {
      copyout(&r, error, sizeof(int));
      return (0);
    }
  }

  e->env_clist[0] = c;
  set_cap_uarea (e, &c);

  return (e->env_id);
}



int
sys_env_free (u_int sn, u_int k, u_int envid) 
  __XOK_SYNC(calls env_access; calls kill_env)
{
  struct Env *e;
  int r;

  if (! (e = env_access (k, envid, ACL_ALL, &r))) return r;
  if (!e->env_id) return -E_BAD_ENV; /* can't free process 0 */
  
  /* can't free something running on another CPU, must revoke first */
#ifdef __SMP__
  if (env_localize(e) < 0) return -E_ENV_RUNNING; 
#endif

  /* printf("environment %d is killed\n",envid); */
  kill_env (e);

  return (0);
}



#ifdef ASH_ENABLE
/* XXX - the following code may very well be broken... */

/* XXX - the following code does not work with the new EnvPD abstraction */

int
sys_env_ashuva (u_int sn, u_int k, u_int envid, u_int va)
{
  u_int va2;
  int npte;
  Pte *pt;
  struct Env *e;
  struct Vpage *vp;
  u_int pteno;
  u_int olduva;

  /* Check access to environment */
  if (!(e = env_access (k, envid, ACL_ALL)) || !e->env_id)
    return (-1);
  /* Check reasonable ASH UVA */
  if ((va & PDMASK) || va >= UTOP)
    return (-1);

  /* If not actually moving ASH, then don't do anything */
  if (va == e->env_ashuva)
    return (0);

  /* punt any scheduling predicate */
  wk_free (e);

  /* Unmap any pages at the new ASH location */
  for (va2 = va; e->env_pd->envpd_nptes[PDENO (va)] > 0 && 
      va2 < va + PDMASK; va2 += NBPG)
    {
      if (ppage_remove (e, va2) < 0)
	sched_runnext();
    }
  if (e->env_pd->envpd_nptes[PDENO (va)] > 0)
    panic ("sys_env_ashuva: number of pte's > # of pages present");

  /* Redo the phys to virt mappings for the ASH's pages */
  /* If the ASH is mapped in... */
  if (e->env_pdir[PDENO (e->env_ashuva)] & PG_P)
    {
      /* # of pages to be redone */
      npte = e->env_pd->envpd_nptes[PDENO (e->env_ashuva)];
      /* Get the page table for this pde */
      pt = ptov (e->env_pdir[PDENO (e->env_ashuva)] & ~PGMASK);
      /* For each ASH page... */
      for (pteno = 0; npte > 0; pteno++)
	{
	  /* Error if we've a higher npte than actual number of mapped pages */
	  if (pteno >= NLPG)
	    panic ("sys_env_ashuva: ran out of valid pages");
	  /* If a page is present... */
	  if (pt[pteno] & PG_P)
	    {
	      /* For each vp pointed to by ppage entry... */
	      for (vp = KLIST_KPTR 
		          (&((Ppage_pp_vpl_get(ppages_get(pt[pteno]>>PGSHIFT)))
			     ->lh_first));
		   ;
		   vp = KLIST_KPTR (&vp->vp_link.le_next))
		{
		  if (!vp)
		    panic ("sys_env_ashuva: could not find vpage");
		  /* If the virtual page reference is to this environment and
		   * the va is in the old ASH address range... */
		  if (vp->vp_env == e->env_id &&
		      (vp->vp_va & ~PDMASK) == e->env_ashuva)
		    {
		      /* set the pointer to the NEW page */
		      vp->vp_va = va + (vp->vp_va & PDMASK);
		      break;
		    }
		}
	      npte--;
	    }
	}
    }

  /* Set the pdirs and npte's for the old and new locations */
  olduva = e->env_ashuva;
  e->env_pdir[PDENO (va)] = e->env_pdir[PDENO (e->env_ashuva)];
  e->env_pd->envpd_nptes[PDENO (va)] = 
    e->env_pd->envpd_nptes[PDENO (e->env_ashuva)];
  e->env_ashuva = va;
  e->env_pdir[PDENO (olduva)] = 0;
  e->env_pd->envpd_nptes[PDENO (olduva)] = 0;

  return (0);
}

#endif /* ASH_ENABLE */


u_int
sys_geteid (u_int sn)
{
  return (curenv->env_id);
}


int
sys_rdu (u_int sn, int k, u_int envid, struct Uenv *ue) 
  __XOK_NOSYNC
{
  struct Env *e;
  int r;

  if (!(e = env_access (k, envid, 0, &r))) return r;
  if (!e->env_u) return -E_BAD_ENV;

  copyout (e->env_u, ue, sizeof (*ue));
  return (0);
}


int
sys_wru (u_int sn, int k, u_int envid, struct Uenv *ue) 
  __XOK_SYNC(calls env_access; attempts env_localize)
{
  struct Env *e;
  int r;

  if (! (e = env_access (k, envid, ACL_ALL, &r))) return r;
  if (! e->env_u) return -E_BAD_ENV;

#ifdef __SMP__
  if (env_localize(e) < 0) return -E_ENV_RUNNING;
#endif

  copyin (ue, e->env_u, sizeof (*ue));

#ifdef __SMP__
  env_release(e);
#endif

  return (0);
}


int
sys_wrenv (u_int sn, int k, u_int envid, struct Env *ue)
  __XOK_SYNC(calls env_access; localize ue and e)
{
  struct Env *e;
  int m, r;

  if (!(e = env_access (k, envid, ACL_ALL, &r)))
    return r;

#ifdef __SMP__
  if (env_localize(e) < 0) 
    return -E_ENV_RUNNING;
  if (env_localize(ue) < 0) 
  {
    env_release(e);
    return -E_ENV_RUNNING;
  }
#endif

  /* Copy everything but segment registers and eflags in the trapframe */
  m = page_fault_mode;
  page_fault_mode = PFM_PROP;

  ue = trup (ue);
  bcopy (&ue->env_tf.tf_edi, &e->env_tf.tf_edi,
	 tf_size (tf_edi, tf_es));
  e->env_tf.tf_eip = ue->env_tf.tf_eip;
  e->env_tf.tf_esp = ue->env_tf.tf_esp;
  /* bcopy the user writeable part */
  bcopy (&ue->env_tf.tf_hi, &e->env_tf.tf_hi,
	 env_offset (env_rdonly) - env_offset (env_tf.tf_hi));


  page_fault_mode = m;

#ifdef __SMP__
  env_release(ue);
  env_release(e);
#endif

  return (0);
}


#ifdef __HOST__
int
sys_set_tss (u_int sn, u_int k, u_int16_t gdt_selector)
{
    struct Ts *tss;
    struct seg_desc *sd;
    u_int tmp_esp0, tmp_cr3;
    u_short tmp_ss0;

    sd = &gdt[gdt_selector>>3];

    /* FIXME: do checks... */

    tss = (struct Ts *)((sd->sd_base_31_24 << 24) | (sd->sd_base_23_16 << 16) | sd->sd_base_15_0);
    if (! (isreadable_varange ((u_int) tss, sizeof (*tss)))) {
	return -E_FAULT;
    }

    tmp_esp0 = p0ts[0].ts_esp0;
    tmp_ss0 = p0ts[0].ts_ss0;
    tmp_cr3 = p0ts[0].ts_cr3;
    memcpy(&p0ts[0], tss, sizeof(p0ts[0]));
    p0ts[0].ts_esp0 = tmp_esp0;
    p0ts[0].ts_ss0 = tmp_ss0;
    p0ts[0].ts_cr3 = tmp_cr3;

    return 0;
}
#endif

#ifdef __ENCAP__
#include <xok/pmapP.h>
#include <xok/sysinfoP.h>
#endif

