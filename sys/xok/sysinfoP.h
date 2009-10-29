
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


#ifndef _SYSINFO_P_H_
#define _SYSINFO_P_H_

/* implements the specification and interface in sysinfo.h */
#include <xok/sysinfo.h>
#include <xok/mplock.h>


/************************
 **   struct Sysinfo   **
 ************************/

struct Sysinfo {
  uint64 si_percpu_ticks[NR_CPUS];	/* current tick count */
#define si_system_ticks si_percpu_ticks[0]  /* system tick count from cpu 0 */
  uint64 si_percpu_idle_ticks[NR_CPUS]; /* number of idle ticks */

  unsigned int si_rate;		/* number of microseconds per tick */
  unsigned int si_mhz;		/* mhz of cpu */
  u_long si_startup_time;	/* si_startup_time+si_system_ticks*si_rate /10^6
				   = current time */

  u_int si_nppages;		/* total number of pages */
  u_int si_ndpages;		/* number of dirty pages */
  u_int si_nfreepages;		/* Number of free pages */
  u_int si_nfreebufpages;	/* Number of valid bc entries marked as free */
  u_int si_pinnedpages;		/* Number of user/bc pages pinned by kernel */


  u_int si_num_nettaps;		/* total number of nettaps in place */
  u_int si_nnetworks;		/* number of de cards */
  struct network si_networks[XOKNET_MAXNETS];


  unsigned long long si_diskintrs;
  u_int si_ndisks;		/* xok's count of disks (excludes off-line) */
  u_int si_ndevs;		/* driver's count of disks (includes off-line) */
  u_int si_scsictlr_cnt;	/* number of active SCSI controllers */
  u_int si_scsictlr_irqs[MAX_SCSI_CTLR_CNT];  /* IRQs for the SCSI controllers */
  struct sd_softc si_devs[MAX_DISKS];	/* disk driver's per-device state */
  struct disk si_disks[MAX_DISKS];
  struct Xn_name si_pxn[MAX_DISKS];	/* proxy xnodes for entire disks */


  u_int si_xn_blocks[MAX_DISKS];	/* number of XN blocks for each disk */
  u_int si_xn_freeblks[MAX_DISKS];	/* number of free XN blocks per disk */
  u_int si_xn_entries;		/* number of entries in xr */

  u_int si_kbd_nchars;		/* number of characters in kbd queue */
  u_int si_crt_pg;		/* page crt buffer is on */
  char si_osid[MAXOSID];


  /* interrupt statistics */
  uint64 si_intr_stats[MAX_IRQS][NR_CPUS];


  /* incremented everytime a process dies */
  uint64 si_killed_envs;

  /* kernel spinlocks */
  struct kspinlock si_global_slocks[NR_GLOBAL_SLOCKS];
  struct kqueuelock si_global_qlocks[NR_GLOBAL_QLOCKS];
};

#ifdef KERNEL

/*
 * generic functions open to all 
 */
STRUCT_SIZEOF(Sysinfo)

FIELD_SIMPLE_READER(Sysinfo,si_system_ticks,uint64)

FIELD_PTR_READER(Sysinfo,si_system_ticks,uint64*)

ARRAY_SIMPLE_READER(Sysinfo,si_percpu_ticks,uint64)

ARRAY_SIMPLE_READER(Sysinfo,si_percpu_idle_ticks,uint64)

FIELD_SIMPLE_READER(Sysinfo,si_rate,unsigned int)

FIELD_SIMPLE_READER(Sysinfo,si_mhz,unsigned int)

FIELD_SIMPLE_READER(Sysinfo,si_startup_time,u_long)

FIELD_SIMPLE_READER(Sysinfo,si_nppages,u_int)

FIELD_SIMPLE_READER(Sysinfo,si_ndpages,u_int)

FIELD_SIMPLE_READER(Sysinfo,si_nfreepages,u_int)

FIELD_SIMPLE_READER(Sysinfo,si_nfreebufpages,u_int)

FIELD_SIMPLE_READER(Sysinfo,si_pinnedpages,u_int)

FIELD_SIMPLE_READER(Sysinfo,si_num_nettaps,u_int)

FIELD_SIMPLE_READER(Sysinfo,si_nnetworks,u_int)

FIELD_SIMPLE_READER(Sysinfo,si_diskintrs,uint64)

FIELD_SIMPLE_READER(Sysinfo,si_ndisks,u_int)

FIELD_SIMPLE_READER(Sysinfo,si_ndevs,u_int)

FIELD_SIMPLE_READER(Sysinfo,si_scsictlr_cnt,u_int)

ARRAY_SIMPLE_READER(Sysinfo,si_scsictlr_irqs,u_int)

ARRAY_SIMPLE_READER(Sysinfo,si_xn_blocks,u_int)

ARRAY_SIMPLE_READER(Sysinfo,si_xn_freeblks,u_int)

FIELD_SIMPLE_READER(Sysinfo,si_xn_entries,u_int)

FIELD_SIMPLE_READER(Sysinfo,si_kbd_nchars,u_int)

FIELD_SIMPLE_READER(Sysinfo,si_crt_pg,u_int)

FIELD_SIMPLE_READER(Sysinfo,si_osid, char*)

FIELD_SIMPLE_READER(Sysinfo,si_killed_envs,uint64)


/*
 * functions open to specific module only
 */

#if defined(__PIC_MODULE__) 
ARRAY_SIMPLE_READER(Sysinfo,si_intr_stats,uint64*)
#endif /* __PIC_MODULE__ */


#if defined(__MPLOCK_MODULE__) 
#ifdef __SMP__
FIELD_SIMPLE_READER(Sysinfo,si_global_slocks,struct kspinlock *)
FIELD_SIMPLE_READER(Sysinfo,si_global_qlocks,struct kqueuelock *)
#endif
#endif /* __MPLOCK_MODULE__ */


#if defined(__SCHED_MODULE__) 
ARRAY_ASSIGN(Sysinfo,si_percpu_ticks,uint64)

ARRAY_ASSIGN(Sysinfo,si_percpu_idle_ticks,uint64)
#endif /* __SCHED_MODULE__ */


#if defined(__KCLOCK_MODULE__) 
FIELD_ASSIGN(Sysinfo,si_rate,unsigned int)

FIELD_ASSIGN(Sysinfo,si_mhz,unsigned int)
#endif /* __KCLOCK_MODULE__ */


#if defined(__BC_MODULE__) 
static inline void Sysinfo_si_ndpages_atomic_inc(struct Sysinfo *s)
{
  asm volatile("lock\n\tincl %0\n" : "=m" (s->si_ndpages) : "0" (s->si_ndpages));
}

static inline void Sysinfo_si_ndpages_atomic_dec(struct Sysinfo *s)
{
  asm volatile("lock\n\tdecl %0\n" : "=m" (s->si_ndpages) : "0" (s->si_ndpages));
}
#endif /* __BC_MODULE__ */


#if defined(__PMAP_MODULE__) 
FIELD_ASSIGN(Sysinfo,si_nppages,u_int)

static inline void Sysinfo_si_nfreepages_atomic_inc(struct Sysinfo *s)
{
  asm volatile("lock\n\tincl %0\n" : "=m" (s->si_nfreepages) : "0" (s->si_nfreepages));
}

static inline void Sysinfo_si_nfreepages_atomic_dec(struct Sysinfo *s)
{
  asm volatile("lock\n\tdecl %0\n" : "=m" (s->si_nfreepages) : "0" (s->si_nfreepages));
}

static inline void Sysinfo_si_nfreebufpages_atomic_inc(struct Sysinfo *s)
{
  asm volatile("lock\n\tincl %0\n" : "=m" (s->si_nfreebufpages) : "0" (s->si_nfreebufpages));
}

static inline void Sysinfo_si_nfreebufpages_atomic_dec(struct Sysinfo *s)
{
  asm volatile("lock\n\tdecl %0\n" : "=m" (s->si_nfreebufpages) : "0" (s->si_nfreebufpages));
}

static inline void Sysinfo_si_pinnedpages_atomic_inc(struct Sysinfo *s)
{
  asm volatile("lock\n\tincl %0\n" : "=m" (s->si_pinnedpages) : "0" (s->si_pinnedpages));
}

static inline void Sysinfo_si_pinnedpages_atomic_dec(struct Sysinfo *s)
{
  asm volatile("lock\n\tdecl %0\n" : "=m" (s->si_pinnedpages) : "0" (s->si_pinnedpages));
}

#endif /* __PMAP_MODULE__ */


#if defined(__NETWORK_MODULE__) 
FIELD_SIMPLE_READER(Sysinfo,si_networks,struct network *)

ARRAY_PTR_READER(Sysinfo,si_networks,struct network *)

static inline void Sysinfo_si_num_nettaps_atomic_inc(struct Sysinfo *s)
{
  asm volatile("lock\n\tincl %0\n" : "=m" (s->si_num_nettaps) : "0" (s->si_num_nettaps));
}

static inline void Sysinfo_si_num_nettaps_atomic_dec(struct Sysinfo *s)
{
  asm volatile("lock\n\tdecl %0\n" : "=m" (s->si_num_nettaps) : "0" (s->si_num_nettaps));
}

static inline void Sysinfo_si_nnetworks_atomic_inc(struct Sysinfo *s)
{
  asm volatile("lock\n\tincl %0\n" : "=m" (s->si_nnetworks) : "0" (s->si_nnetworks));
}

static inline void Sysinfo_si_nnetworks_atomic_dec(struct Sysinfo *s)
{
  asm volatile("lock\n\tdecl %0\n" : "=m" (s->si_nnetworks) : "0" (s->si_nnetworks));
}

#endif /* __NETWORK_MODULE__ */


#if defined(__DISK_MODULE__) 
ARRAY_PTR_READER(Sysinfo,si_devs,struct sd_softc *)

ARRAY_PTR_READER(Sysinfo,si_disks,struct disk *)

FIELD_SIMPLE_READER(Sysinfo,si_disks,struct disk *)

ARRAY_PTR_READER(Sysinfo,si_pxn,struct Xn_name*)

FIELD_ASSIGN(Sysinfo,si_ndisks,u_int)

FIELD_ASSIGN(Sysinfo,si_ndevs,u_int)
#endif /* __DISK_MODULE__ */

#ifdef __SCSI_MODULE__
FIELD_ASSIGN(Sysinfo,si_diskintrs,uint64)  // scsi disks

FIELD_ASSIGN(Sysinfo,si_scsictlr_cnt,u_int)

ARRAY_ASSIGN(Sysinfo,si_scsictlr_irqs,u_int)
#endif

#ifdef __XN_MODULE__
ARRAY_ASSIGN(Sysinfo,si_xn_blocks,u_int)

ARRAY_ASSIGN(Sysinfo,si_xn_freeblks,u_int)

FIELD_ASSIGN(Sysinfo,si_xn_entries,u_int)
#endif

#ifdef __CONSOLE_MODULE__
FIELD_ASSIGN(Sysinfo,si_kbd_nchars,u_int)

FIELD_ASSIGN(Sysinfo,si_crt_pg,u_int)
#endif

#ifdef __ENV_MODULE__
FIELD_ASSIGN(Sysinfo,si_killed_envs,uint64)
#endif

FIELD_ASSIGN(Sysinfo,si_startup_time,u_long)

#endif /* KERNEL */


/******************
 **  misc stuff  **
 ******************/

#ifdef KERNEL

static inline void SysinfoAssertSize(int size)
{
  StaticAssert(sizeof (struct Sysinfo) <= SYSINFO_SIZE);
}

#endif


#endif /* _SYSINFO_P_H_ */

