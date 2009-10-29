
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


#ifndef __XOK_MP_LOCK_H__
#define __XOK_MP_LOCK_H__


/* define the following flag to include debugging checks in spinlock code */
// #define LOCK_DEBUG 1

#include <xok/xoktypes.h>
#include <machine/param.h>


/* lock numbers for coarsed-grain spinlocks on major system resources */

#define /* 0  */ KERNEL_LOCK		0
#define /* 1  */ NETTAP_LOCK		KERNEL_LOCK+1
#define /* 2  */ PKTQ_LOCK		NETTAP_LOCK+1
#define /* 3  */ PKTRING_LOCK		PKTQ_LOCK+1
#define /* 4  */ KBD_LOCK		PKTRING_LOCK+1
#define /* 5  */ MALLOC_LOCK		KBD_LOCK+1
#define /* 6  */ CONSOLE_LOCK    	MALLOC_LOCK+1
#define /* 7  */ PICIRQ_LOCK		CONSOLE_LOCK+1
#define /* 8 */ UNUSED_LOCK_0		PICIRQ_LOCK+1
#define /* 9 */ UNUSED_LOCK_1		UNUSED_LOCK_0+1
#define /* 10 */ UNUSED_LOCK_2		UNUSED_LOCK_1+1
#define /* 11 */ UNUSED_LOCK_3		UNUSED_LOCK_2+1
#define /* 12 */ UNUSED_LOCK_4		UNUSED_LOCK_3+1

#define NR_GLOBAL_SLOCKS UNUSED_LOCK_4+1

/* lock numbers for global queue based locks */
#define /* 0  */ SYSINFO_LOCK		0
#define /* 1  */ ENV_LIST_LOCK		SYSINFO_LOCK+1
#define /* 2  */ PPAGE_FLIST_LOCK	ENV_LIST_LOCK+1
#define /* 3 */  PPAGE_FBUF_LOCK	PPAGE_FLIST_LOCK+1

#define NR_GLOBAL_QLOCKS PPAGE_FBUF_LOCK+1


struct kspinlock  		/* 16 bytes */
{
  uint16  lock;			/*  2 bytes - the lock itself */
  int16   owner;		/*  2 bytes - CPU holding the lock, or -1 */
  uint32  depth;		/*  4 bytes - recursive locking depth */
  uint64  uses;			/*  8 bytes - usage count */
};

struct kqueuelock
{
  uint16  lock;			/*  2 bytes - the lock itself */
  int16   owner;		/*  2 bytes - CPU holding the lock, or -1 */
  uint32  depth;		/*  4 bytes - recursive locking depth */
  uint64  uses;			/*  8 bytes - usage count */
  uint32  tickets[NR_CPUS][8];  /* 32 bytes (a cache line) for each CPU */
};

struct krwlock
{
  u_int  nreaders;		/*  4 bytes - number of readers */
  int    writer;		/*  4 bytes - CPU holding exclusive lock */
  u_int  depth;			/*  4 bytes - exclusive lock depth */
  uint64 uses;			/*  8 bytes - usage count */
  u_int  unused[3];		/* 12 bytes - unused */
};


#ifdef KERNEL 

/* coarsed grained kernel spinlocks */
extern struct kspinlock* global_slocks;
/* coarsed grained kernel queuelocks */
extern struct kqueuelock* global_qlocks;
/* signal that we are in SMP mode and can start using locks */
extern volatile int smp_commenced;  

#define GLOCK(ln)	(&global_slocks[(ln)])
#define GQLOCK(ln)	(&global_qlocks[(ln)])

#else 

extern struct kspinlock __si_global_slocks[];
extern struct kqueuelock __si_global_qlocks[];

#endif /* KERNEL */

#endif /* __XOK_MP_LOCK_H__ */

