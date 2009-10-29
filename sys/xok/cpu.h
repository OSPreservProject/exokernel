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


#ifndef __XOK_CPU_H__
#define __XOK_CPU_H__


#include <xok/scheduler.h>
#include <xok/ipi.h>

struct Env;

/* 
 * the following data structure contains per-CPU private variables. They are
 * created at start up time in ppage_init and ppage_init_secondary, and are
 * stored in __cpucxts[cpu_id]. They are also mapped at CPUCXT in VA space.
 */

struct CPUContext 
{
  /* !! make the first four variables and tickets be in two cache lines: in
   * spinlocks we check for ipi counts and possibly invoke ipi handler... so
   * we want to not have to suffer a cache miss. since cpu_id is for sure used
   * in those situations, this cache line should already be loaded. */
  u_int _cpu_id;			/* CPU id */
  u_int _last_ticket;			/* idx of last ticket for queue locks */
  u_int _ipi_pending;			/* current number of pending IPIs */
  u_int _in_ipih;			/* in ipi handler */
#define ACTIVE_TICKETS	12
  u_int _ql_tickets[ACTIVE_TICKETS];	/* tickets for queue locks */

  struct Env *_current_env;		/* current environment */
  int _page_fault_mode;			/* page fault mode */
  int _in_revocation;			/* environment being revoked */
  u_int _current_pd_id;			/* id of current pd */
  struct Env *_idle_env;		/* idle env for this cpu */
  int _current_q;			/* current quantum */

  struct Quantum _qvec[NQUANTUM];	/* quantum vector */
  struct Quantum_list _qfreelist;	/* list of free quanta */
  struct kspinlock _qvec_spinlock; 	/* lock for quantum vector */
 
#define IPIQ_SIZE 64
  ipimsg_t _ipiq[IPIQ_SIZE];      	/* LIFO IPI message queue - we just
					 * make this large enough - if we get
					 * more than that many unserviced, we
					 * are probably doomed anyways */
  struct kspinlock _ipi_spinlock;	/* lock on ipi queue */

  u_int _apic_id;			/* APIC id */
  int _page_fault_vcopy;		/* vcopy mode */
  int _ash_va; 				/* ash */
};



#ifdef KERNEL

#include <machine/param.h>

#ifdef __SMP__
extern int cpu_id2count[NR_CPUS];
extern int cpu_count2id[NR_CPUS];
extern volatile int smp_cpu_count;
extern volatile int smp_mode;
extern volatile int smp_commenced;
#include <xok/apic.h>		/* where cpu id comes from */
#endif


#ifdef __SMP__
static inline int 
get_apic_id ()
{
  return GET_APIC_ID (apic_read (APIC_ID));
}
#endif


static inline int 
get_cpu_id ()
  /* id of the calling cpu */
{
#ifdef __SMP__
  return cpu_id2count[get_apic_id ()];
#else
  return 0;
#endif
}


static inline int
get_cpu_count()
  /* get number of cpus */
{
#ifdef __SMP__
  return smp_cpu_count;
#else
  return 1;
#endif
}


#include <xok/mmu.h>

/* this contains ALL cpu context structures */
extern struct CPUContext *__cpucxts[NR_CPUS];

/* per cpu context page tables */
extern Pte *cpucxt_pts[NR_CPUS];

/* per cpu stack page tables */
extern Pte *kstack_pts[NR_CPUS];


static inline int
tr2cpuid()
{
#ifdef __SMP__
  register int r;
  asm volatile("xorl %%eax,%%eax\n"
      	       "\tstr %%ax\n"
	       "\taddl %1,%%eax\n"
	       "\tshrl $3,%%eax\n"
	       "\tmovl %%eax,%0\n" : "=r" (r) : "i" (-GD_TSS) : "%eax");
  return r;
#else
  return 0;
#endif
}


static inline struct CPUContext*
get_cpu_ctx()
{
#if 1
  return (struct CPUContext*) CPUCXT;
#else
  return __cpucxts[tr2cpuid()];
#endif
}


#define cpu_id 	  	  (get_cpu_ctx()->_cpu_id)
#define apic_id	  	  (get_cpu_ctx()->_apic_id)
#define quantum_freelist  (get_cpu_ctx()->_qfreelist)
#define quantums	  (get_cpu_ctx()->_qvec)
#define curq		  (get_cpu_ctx()->_current_q)
#define in_revocation	  (get_cpu_ctx()->_in_revocation)
#define page_fault_mode	  (get_cpu_ctx()->_page_fault_mode)
#define page_fault_vcopy  (get_cpu_ctx()->_page_fault_vcopy)
#define curenv		  (get_cpu_ctx()->_current_env)
#define cur_pd		  (get_cpu_ctx()->_current_pd_id)
#define in_ipih	  	  (get_cpu_ctx()->_in_ipih)
#define ipi_pending	  (get_cpu_ctx()->_ipi_pending)
#define ipiq		  (get_cpu_ctx()->_ipiq)
#define ipi_spinlock	  (get_cpu_ctx()->_ipi_spinlock)
#define ash_va		  (get_cpu_ctx()->_ash_va)
#define last_ticket   	  (get_cpu_ctx()->_last_ticket)
#define ql_tickets        (get_cpu_ctx()->_ql_tickets)
#define qvec_lock(c)	   (__cpucxts[c]->_qvec_spinlock)

#endif /* KERNEL */

#include <xok/apic.h>

#endif /* __XOK_CPU_H__ */


