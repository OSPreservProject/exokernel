
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

/*	$OpenBSD: pctr.h,v 1.5 1996/10/20 15:27:48 dm Exp $	*/

/*
 * Pentium performance counter driver for OpenBSD.
 * Copyright 1996 David Mazieres <dm@lcs.mit.edu>.
 *
 * Modification and redistribution in source and binary forms is
 * permitted provided that due credit is given to the author and the
 * OpenBSD project (for instance by leaving this copyright notice
 * intact).
 */

#ifndef _I386_PCTR_H_
#define _I386_PCTR_H_

extern int usetsc;
extern int usep5ctr;
extern int usep6ctr;

int pctr_init (void);

typedef u_quad_t pctrval;

#define PCTR_NUM 2

struct pctrst {
  u_int pctr_fn[PCTR_NUM];    /* Current settings of hardware counters */
  pctrval pctr_tsc;           /* Free-running 64-bit cycle counter */
  pctrval pctr_hwc[PCTR_NUM]; /* Values of the hardware counters */
  pctrval pctr_idl;           /* Iterations of the idle loop */
};

/* Bit values in fn fields and PIOCS ioctl's */
#define P5CTR_K 0x40          /* Monitor kernel-level events */
#define P5CTR_U 0x80          /* Monitor user-level events */
#define P5CTR_C 0x100         /* count cycles rather than events */

#define P6CTR_U  0x010000     /* Monitor user-level events */
#define P6CTR_K  0x020000     /* Monitor kernel-level events */
#define P6CTR_E  0x040000     /* Edge detect */
#define P6CTR_EN 0x400000     /* Enable counters (counter 0 only) */
#define P6CTR_I  0x800000     /* Invert counter mask */

/* Unit Mask bits */
#define P6CTR_UM_M 0x0800     /* Modified cache lines */
#define P6CTR_UM_E 0x0400     /* Exclusive cache lines */
#define P6CTR_UM_S 0x0200     /* Shared cache lines */
#define P6CTR_UM_I 0x0100     /* Invalid cache lines */
#define P6CTR_UM_MESI (P6CTR_UM_M|P6CTR_UM_E|P6CTR_UM_S|P6CTR_UM_I)
#define P6CTR_UM_A 0x2000     /* Any initiator (as opposed to self) */

#define P6CTR_CM_SHIFT 24     /* Left shift for counter mask */

/* ioctl to set which counter a device tracks. */
/*#define PCIOCRD _IOR('c', 1, struct pctrst)
  #define PCIOCS0 _IOW('c', 8, unsigned int)
  #define PCIOCS1 _IOW('c', 9, unsigned int) */

#define PCIOCRD 1   /* Read counter value */
#define PCIOCS0 8    /* Set counter 0 function */
#define PCIOCS1 9    /* Set counter 1 function */

#define _PATH_PCTR "/dev/pctr"


#define __cpuid()				\
({						\
  pctrval id;					\
  __asm __volatile ("pushfl\n"			\
		    "\tpopl %%eax\n"		\
		    "\tmovl %%eax,%%ecx\n"	\
		    "\txorl %1,%%eax\n"		\
		    "\tpushl %%eax\n"		\
		    "\tpopfl\n"			\
		    "\tpushfl\n"		\
		    "\tpopl %%eax\n"		\
		    "\tpushl %%ecx\n"		\
		    "\tpopfl\n"			\
		    "\tcmpl %%eax,%%ecx\n"	\
		    "\tmovl $0,%%eax\n"		\
		    "\tje 1f\n"			\
		    "\tcpuid\n"			\
		    "\ttestl %%eax,%%eax\n"	\
		    "\tje 1f\n"			\
		    "\tmovl $1,%%eax\n"		\
		    "\tcpuid\n"			\
		    "1:\t"			\
		    : "=A" (id) : "i" (FL_ID)	\
		    : "edx", "ecx", "ebx");	\
  id;						\
})

#define __hastsc(id) (((id) & 0x1000000000ULL) != 0ULL)
#define __hasp5ctr(id) (((id) & 0xf00) == 0x500		\
			&& (((id) & 0xf0) == 0x10	\
			    || ((id) & 0xf0) == 0x20))
#define __hasp6ctr(id) (((id) & 0xf00) == 0x600)		

#define __cpufamily() ((__cpuid() >> 8) & 0xf)

#define rdtsc()						\
({							\
  pctrval v;						\
  __asm __volatile (".byte 0xf, 0x31" : "=A" (v));	\
  v;							\
})

/* Read the performance counters (Pentium Pro only) */
#define rdpmc(ctr)				\
({						\
  pctrval v;					\
  __asm __volatile (".byte 0xf, 0x33\n"		\
		    "\tandl $0xff, %%edx"	\
		    : "=A" (v) : "c" (ctr));	\
  v;						\
})

/*#define CR4_TSD 0x040*/         /* Time stamp disable */
/*#define CR4_PCE 0x100*/         /* Performance counter enable */

#define MSR_TSC 0x10          /* MSR for TSC */
#define P5MSR_CTRSEL 0x11     /* MSR for selecting both counters on P5 */
#define P5MSR_CTR0 0x12       /* Value of Ctr0 on P5 */
#define P5MSR_CTR1 0x13       /* Value of Ctr1 on P5 */
#define P6MSR_CTRSEL0 0x186   /* MSR for programming CTR0 on P6 */
#define P6MSR_CTRSEL1 0x187   /* MSR for programming CTR0 on P6 */
#define P6MSR_CTR0 0xc1       /* Ctr0 on P6 */
#define P6MSR_CTR1 0xc2       /* Ctr1 on P6 */

#define rdmsr(msr)						\
({								\
  pctrval v;							\
  __asm __volatile (".byte 0xf, 0x32" : "=A" (v) : "c" (msr));	\
  v;								\
})

#define wrmsr(msr, v) \
     __asm __volatile (".byte 0xf, 0x30" :: "A" ((u_quad_t) (v)), "c" (msr));

#endif /* ! _I386_PCTR_H_ */
