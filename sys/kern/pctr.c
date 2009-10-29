
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

/*      $OpenBSD: pctr.c,v 1.7 1996/10/20 15:27:53 dm Exp $     */

/*
 * Pentium performance counter driver for OpenBSD.
 * Copyright 1996 David Mazieres <dm@lcs.mit.edu>.
 *
 * Modification and redistribution in source and binary forms is
 * permitted provided that due credit is given to the author and the
 * OpenBSD project (for instance by leaving this copyright notice
 * intact).
 */

#include <xok/types.h>
#include <xok/printf.h>
#include <xok/pctr.h>
#include <xok/sys_proto.h>

pctrval pctr_idlcnt;  /* NOTE: idle count not currently incremented */

int usep5ctr;
int usep6ctr;
int usetsc;

int 
pctr_init (void)
{
  pctrval id = __cpuid ();
  usetsc = __hastsc (id);
  usep5ctr = __hasp5ctr (id);
  usep6ctr = __hasp6ctr (id);

  if (usep6ctr)
  {
    /* Enable RDTSC and RDPMC instructions from user-level. */
    asm volatile (".byte 0xf,0x20,0xe0   # movl %%cr4,%%eax\n"
		  "\tandl %0,%%eax\n"
		  "\torl %1,%%eax\n"
		  "\t.byte 0xf,0x22,0xe0 # movl %%cr4,%%eax"
		  :: "i" (~CR4_TSD), "i" (CR4_PCE) : "eax");
    printf ("  Enabled user-level rdtsc and rdpmc\n");
  }

  if (usetsc) {
#if 0
    asm volatile (".byte 0xf,0x20,0xe0   # movl %%cr4,%%eax\n"
                  "\tandl %0,%%eax\n"
                  "\t.byte 0xf,0x22,0xe0 # movl %%cr4,%%eax"
                  :: "i" (~CR4_TSD) : "eax");
#endif
    wrmsr (0x10, 0);
    printf ("  Zeroed cycle counter\n");
  }

  return 0;
}

#if 0
void
pctr_init (void)
{
  pctrval id = __cpuid ();
  int usetsc = __hastsc (id);
  /* int usep5ctr = __hasp5ctr (id); */
  int usep6ctr = __hasp6ctr (id);

  if (usep6ctr)
    /* Enable RDTSC and RDPMC instructions from user-level. */
    asm volatile (".byte 0xf,0x20,0xe0   # movl %%cr4,%%eax\n"
		  "\tandl %0,%%eax\n"
		  "\torl %1,%%eax\n"
		  "\t.byte 0xf,0x22,0xe0 # movl %%cr4,%%eax"
		  :: "i" (~CR4_TSD), "i" (CR4_PCE) : "eax");
  if (usetsc) {
    wrmsr (0x10, 0);
    printf ("  Zeroed cycle counter\n");
  }
}
#endif

static int
p5ctrsel (int fflag, u_int cmd, u_int fn)
{
  pctrval msr11;
  int msr;
  int shift;

  cmd -= PCIOCS0;
  if (cmd > 1) {
      printf("p5ctrsel: invalid command %x\n", cmd);
      return -1;
  }
  msr = P5MSR_CTR0 + cmd;
  shift = cmd ? 0x10 : 0;

  if (fn >= 0x200) {
      printf("p5ctrsel: bad function %x\n", fn);
      return -1;
  }

  msr11 = rdmsr (0x11);
  msr11 &= ~(0x1ffLL << shift);
  msr11 |= fn << shift;
  wrmsr (0x11, msr11);
  wrmsr (msr, 0);

  return 0;
}

static inline void
p5ctrrd (struct pctrst *st)
{
  u_int msr11;

  msr11 = rdmsr (P5MSR_CTRSEL);
  st->pctr_fn[0] = msr11 & 0xffff;
  st->pctr_fn[1] = msr11 >> 16;

  __asm __volatile ("cli");
  st->pctr_tsc = rdtsc ();
  st->pctr_hwc[0] = rdmsr (P5MSR_CTR0);
  st->pctr_hwc[1] = rdmsr (P5MSR_CTR1);
  __asm __volatile ("sti");
}

static int
p6ctrsel (int fflag, u_int cmd, u_int fn)
{
  int msrsel, msrval;

  cmd -= PCIOCS0;
  if (cmd > 1) {
      printf("p6crlsel: invalid command %x\n",cmd);
      return -1;
  }
  msrsel = P6MSR_CTRSEL0 + cmd;
  msrval = P6MSR_CTR0 + cmd;

  if (fn & 0x380000){
      printf("p6ctrsel: invalid function %x\n",fn);
      return -1;
  }

  wrmsr (msrval, 0);
  wrmsr (msrsel, fn);
  wrmsr (msrval, 0);

  return 0;
}

static inline void
p6ctrrd (struct pctrst *st)
{
  st->pctr_fn[0] = rdmsr (P6MSR_CTRSEL0);
  st->pctr_fn[1] = rdmsr (P6MSR_CTRSEL1);
  __asm __volatile ("cli");
  st->pctr_tsc = rdtsc ();
  st->pctr_hwc[0] = rdpmc (0);
  st->pctr_hwc[1] = rdpmc (1);
  __asm __volatile ("sti");
}

int 
sys_pctr (u_int sn, u_int cmd, u_int fflag, void *data)
{
    extern void bzero (void *, unsigned int);
    /* printf("sn = %x, cmd = %x, fflag = %x, data = %x\n",
       sn, cmd, fflag, (u_int) data); */
    switch (cmd) {
      case PCIOCRD:
      {
	  struct pctrst *st = (void *) data;
	  
	  if (usep6ctr)
	      p6ctrrd (st);
	  else if (usep5ctr)
	      p5ctrrd (st);
	  else {
	      bzero (st, sizeof (*st));
	      if (usetsc)
		  st->pctr_tsc = rdtsc ();
	  }
	  st->pctr_idl = pctr_idlcnt;
	  return 0;
      }
      case PCIOCS0:
      case PCIOCS1:
	  if (usep6ctr)
	      return p6ctrsel (fflag, cmd, *((u_int *) data));
	  if (usep5ctr) {
	      /* printf("data = %x, *data = %x\n", (u_int) data, 
		 (u_int) *((u_int *)data)); */
	      return p5ctrsel (fflag, cmd, *((u_int *) data));
	  }
	  else {
	      printf("no pctrs with this machine\n");
	      return -1;
	  }
      default:
	  printf("unknown request: %x\n", cmd);
	  return -1;
    }
}
