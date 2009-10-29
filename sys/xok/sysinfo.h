
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


#ifndef _SYSINFO_H_
#define _SYSINFO_H_

/*
 * this file contains specifications and interfaces for the sysinfo
 * data structure.
 */

#include <machine/param.h>
#include <xok/types.h>
#include <xok/disk.h>
#include <xok/network.h>
#include <xok/pxn.h>
#include <xok/sd.h>
#include <xok/picirq.h>	/* MAX_IRQS */
#include <xok/mplock.h>
#include <xok/sysinfo_decl.h>


/* contains macros that produce data encapsulation inline functions */
#include "../tools/data-encap/encap.h"

 
 
/************************
 **   struct Sysinfo   **
 ************************/

struct Sysinfo;

#ifdef KERNEL

/*
 * general functions open to everyone
 */

/* returns size of Sysinfo */
STRUCT_SIZEOF_DECL(Sysinfo)

/* returns Sysinfo->si_system_ticks */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_system_ticks,uint64)

/* returns &Sysinfo->si_system_ticks */
FIELD_PTR_READER_DECL(Sysinfo,si_system_ticks,uint64*)

/* returns Sysinfo->si_percpu_ticks[i] */
ARRAY_SIMPLE_READER_DECL(Sysinfo,si_percpu_ticks,uint64)

/* returns Sysinfo->si_percpu_idle_ticks[i] */
ARRAY_SIMPLE_READER_DECL(Sysinfo,si_percpu_idle_ticks,uint64)

/* returns Sysinfo->si_rate */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_rate,unsigned int)

/* returns Sysinfo->si_mhz */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_mhz,unsigned int)

/* returns Sysinfo->si_startup_time */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_startup_time,u_long)

/* returns Sysinfo->si_nppages */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_nppages,u_int)

/* returns Sysinfo->si_ndpages */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_ndpages,u_int)

/* returns Sysinfo->si_nfreepages */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_nfreepages,u_int)

/* returns Sysinfo->si_nfreebufpages */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_nfreebufpages,u_int)

/* returns Sysinfo->si_pinnedpages */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_pinnedpages,u_int)

/* returns Sysinfo->si_num_nettaps */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_num_nettaps,u_int)

/* returns Sysinfo->si_nnetworks */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_nnetworks,u_int)

/* returns Sysinfo->si_diskintrs */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_diskintrs,uint64)

/* returns Sysinfo->si_ndisks */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_ndisks,u_int)

/* returns Sysinfo->si_ndevs */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_ndevs,u_int)

/* returns Sysinfo->si_scsictlr_cnt */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_scsictlr_cnt,u_int)

/* returns Sysinfo->si_scsictlr_irqs[i] */
ARRAY_SIMPLE_READER_DECL(Sysinfo,si_scsictlr_irqs,u_int)

/* returns Sysinfo->si_xn_blocks */
ARRAY_SIMPLE_READER_DECL(Sysinfo,si_xn_blocks,u_int)

/* returns Sysinfo->si_xn_freeblks */
ARRAY_SIMPLE_READER_DECL(Sysinfo,si_xn_freeblks,u_int)

/* returns Sysinfo->si_xn_entries */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_xn_entries,u_int)

/* returns Sysinfo->si_kbd_nchars */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_kbd_nchars,u_int)

/* returns Sysinfo->si_crt_pg */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_crt_pg,u_int)

/* returns Sysinfo->si_osid */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_osid, char*)

/* returns Sysinfo->si_killed_envs */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_killed_envs,uint64)

/*
 * functions open to specific module only
 */

#if defined(__PIC_MODULE__) 
/* returns Sysinfo->si_intr_stats[irq], which is an array indexed by CPU # */
ARRAY_SIMPLE_READER_DECL(Sysinfo,si_intr_stats,uint64*)
#endif /* __PIC_MODULE__ */


#if defined(__MPLOCK_MODULE__) 
#ifdef __SMP__
/* returns Sysinfo->si_global_slocks */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_global_slocks,struct kspinlock *)
/* returns Sysinfo->si_global_qlocks */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_global_qlocks,struct kqueuelock *)
#endif
#endif /* __MPLOCK_MODULE__ */


#if defined(__SCHED_MODULE__)
ARRAY_ASSIGN_DECL(Sysinfo,si_percpu_ticks,uint64)

ARRAY_ASSIGN_DECL(Sysinfo,si_percpu_idle_ticks,uint64)
#endif /* __SCHED_MODULE__ */


#if defined(__KCLOCK_MODULE__) 
FIELD_ASSIGN_DECL(Sysinfo,si_rate,unsigned int)

FIELD_ASSIGN_DECL(Sysinfo,si_mhz,unsigned int)
#endif /* __KCLOCK_MODULE__ */


#if defined(__BC_MODULE__) 
static inline void Sysinfo_si_ndpages_atomic_inc(struct Sysinfo *);
static inline void Sysinfo_si_ndpages_atomic_dec(struct Sysinfo *);
#endif /* __BC_MODULE__ */


#if defined(__PMAP_MODULE__) 
FIELD_ASSIGN_DECL(Sysinfo,si_nppages,u_int)

static inline void Sysinfo_si_nfreepages_atomic_inc(struct Sysinfo *);
static inline void Sysinfo_si_nfreepages_atomic_dec(struct Sysinfo *);
static inline void Sysinfo_si_nfreebufpages_atomic_inc(struct Sysinfo *);
static inline void Sysinfo_si_nfreebufpages_atomic_dec(struct Sysinfo *);
static inline void Sysinfo_si_pinnedpages_atomic_inc(struct Sysinfo *);
static inline void Sysinfo_si_pinnedpages_atomic_dec(struct Sysinfo *);
#endif /* __PMAP_MODULE__ */


#if defined(__NETWORK_MODULE__) 
/* returns Sysinfo->si_networks */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_networks,struct network *)

/* returns &Sysinfo->si_networks[i] */
ARRAY_PTR_READER_DECL(Sysinfo,si_networks,struct network *)

static inline void Sysinfo_si_num_nettaps_atomic_dec(struct Sysinfo *);
static inline void Sysinfo_si_num_nettaps_atomic_inc(struct Sysinfo *);

static inline void Sysinfo_si_nnetworks_atomic_inc(struct Sysinfo *);
static inline void Sysinfo_si_nnetworks_atomic_dec(struct Sysinfo *);
#endif /* __NETWORK_MODULE__ */


#if defined(__DISK_MODULE__) 
/* returns &Sysinfo->si_devs[i] */
ARRAY_PTR_READER_DECL(Sysinfo,si_devs,struct sd_softc *)

/* returns &Sysinfo->si_disks[i] */
ARRAY_PTR_READER_DECL(Sysinfo,si_disks,struct disk *)

/* returns Sysinfo->si_disks */
FIELD_SIMPLE_READER_DECL(Sysinfo,si_disks,struct disk *)

/* returns &Sysinfo->si_pxn[i] */
ARRAY_PTR_READER_DECL(Sysinfo,si_pxn,struct Xn_name*)

FIELD_ASSIGN_DECL(Sysinfo,si_ndisks,u_int)

FIELD_ASSIGN_DECL(Sysinfo,si_ndevs,u_int)
#endif /* __DISK_MODULE__ */


#ifdef __SCSI_MODULE__
FIELD_ASSIGN_DECL(Sysinfo,si_diskintrs,uint64)  // scsi disks

FIELD_ASSIGN_DECL(Sysinfo,si_scsictlr_cnt,u_int)

ARRAY_ASSIGN_DECL(Sysinfo,si_scsictlr_irqs,u_int)
#endif

#ifdef __XN_MODULE__
ARRAY_ASSIGN_DECL(Sysinfo,si_xn_blocks,u_int)

ARRAY_ASSIGN_DECL(Sysinfo,si_xn_freeblks,u_int)

FIELD_ASSIGN_DECL(Sysinfo,si_xn_entries,u_int)
#endif

#ifdef __CONSOLE_MODULE__
FIELD_ASSIGN_DECL(Sysinfo,si_kbd_nchars,u_int)

FIELD_ASSIGN_DECL(Sysinfo,si_crt_pg,u_int)
#endif

#ifdef __ENV_MODULE__
FIELD_ASSIGN_DECL(Sysinfo,si_killed_envs,uint64)
#endif

FIELD_ASSIGN_DECL(Sysinfo,si_startup_time,u_long)

#endif /* KERNEL */





/******************************
 **   global sysinfo table   **
 ******************************/

#ifdef KERNEL    /* kernel version */

extern struct Sysinfo *si;

#define SYSINFO_PTR(field) Sysinfo_##field##_ptr(si)
#define SYSINFO_PTR_AT(field, i) Sysinfo_##field##_ptr_at(si,i)

#define SYSINFO_GET(field) Sysinfo_##field##_get(si)
#define SYSINFO_GET_AT(field, i) Sysinfo_##field##_get_at(si,i)

/* assign value to field */
#define SYSINFO_ASSIGN(field,val) Sysinfo_##field##_set(si,val)

/* assign value to field[i] */
#define SYSINFO_A_ASSIGN(field,i,val) Sysinfo_##field##_set_at(si,i,val)

#else            /* exposed to libOS */

extern struct Sysinfo __sysinfo;

#endif           /* !KERNEL */




/********************************
 **   misc SI definitions   **
 ********************************/

#ifdef KERNEL

#define SI_MAKETIME \
  (SYSINFO_GET(si_startup_time) + \
   (SYSINFO_GET_AT(si_percpu_ticks,0) * SYSINFO_GET(si_rate)) / 1000000)

#define SI_HZ (1000000 / SYSINFO_GET(si_rate)) /* ticks per second */

static inline void SysinfoAssertSize(int size);

#endif /* KERNEL */

#endif /* _SYSINFO_H_ */


#if !defined(__ENCAP__) || !defined(KERNEL)
#include <xok/sysinfoP.h>
#endif

