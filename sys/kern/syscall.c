
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

#define __VIA_SYSCALL__

#include <xok/sysinfo.h>
#include <xok/env.h>
#include <xok/sys_proto.h>
#include <xok/kerrno.h>
#include <xok/console.h>
#include <xok/printf.h>
#include <xok/pctr.h>
#include <xok/mplock.h>



/* count of syscalls made. make sure to define the incl in locore.S
   that actually increments this.... */

extern unsigned int nsyscalls;

u_int
sys_get_nsyscalls (u_int sn)
{
  return nsyscalls;
}


  
void
sys_null (u_int sn)
{
}



/* If set, called by kernel page fault code in case of user page fault
   during a syscall.  This gives the system call a chance to cleanup.
   The three arguments are: faulting va, errcode, and trapno.
   It is reset to NULL by syscall trap code before each syscall.
*/
extern void (*syscall_pfcleanup)(u_int, u_int, u_int);


int
syscall_init (void)
{
  int i;

  for (i = 0; i < MAX_SYSCALL; i++)
    if (! sctab[i].sc_fn)
      sctab[i].sc_fn = (void *) sys_badcall;

#ifdef __SMP__
  {
    extern void syscall_perf_init();
    syscall_perf_init();
  }
#endif
  return 0;
}


/* allow user to get ring 0 access, must have zero length capability with
   all permissions */
int
sys_ring0 (u_int sn, u_int k, void *h)
{
  cap c;
  int r;

  if ((r = env_getcap (curenv, k, &c)) < 0)
    return (r);
  if (c.c_isptr || c.c_len || c.c_perm != CL_ALL)
    return (-E_CAP_INSUFF);

  /* call user supplied function */
  ((void (*)())h)();
  return 0;
}

/* clear the TS bit in cr0 so that user can use fpu */
void
sys_clts (u_int sn)
{
  asm ("clts\n");
}

void
sys_tstamp (u_int sn)
{
  uint64 t = SYSINFO_GET(si_system_ticks);
  printf ("Time:  0x%08x%08x\n", ((u_int *)&t)[1], ((u_int *)&t)[0]);
}

void
sys_cputs (u_int sn, char *s)
{
  if (0)
  {
    pfm (PFM_PROP, printf ("%d: %s", curenv->env_id, trup (s)));
  }
  else
  {
    pfm (PFM_PROP, printf ("%s", trup (s)));
  }
}

/* GROK -- this probably needs to be augmented with some access control. */
/* not clear we want all processes to have unlimited read access to the  */
/* console...                                                            */
int
sys_copy_console_buffer (u_int sn, char *buf, int maxlen)
{
  int ret;
  pfm (PFM_PROP, ret = cb_copybuf (trup (buf), maxlen));
  return (ret);
}


void
sys_badcall (u_int sn)
{
  printf ("Invalid system call 0x%x\n", sn);
}

void
sys_argtest (u_int sn, int a1, int a2, int a3, int a4, int a5, int a6)
{
  printf ("Testing syscall 0x%x args:\n"
	  "  %d, %d, %d, %d, %d %d\n", sn, a1, a2, a3, a4, a5, a6);
}

int
sys_startup_time (u_int sn, int k, u_long t)
{
  int ret;
  cap c;
  
  ret = env_getcap (curenv, k, &c);
  if (ret < 0) return ret;

  if (c.c_perm != CL_ALL || c.c_len) return (-E_CAP_INSUFF);

  SYSINFO_ASSIGN(si_startup_time, t);
  resettodr(); /* updates the cmos clock */
  return (0);
}



volatile u_int uinfo_pgno = 0;
void
sys_uinfo_set(u_int sn, u_int k, u_int pgno)
{
  uinfo_pgno = pgno;
}

u_int
sys_uinfo_get(u_int sn)
{
  return uinfo_pgno;
}


#ifdef __HOST__
#include <xok/init.h>
int
sys_set_gdt (u_int sn, u_int k, u_int i, struct seg_desc *_sd)
{
    struct seg_desc sd;
    int m;

    if (i==0 || i>=GD_NULLS)
	return -E_INVAL;

    m = page_fault_mode;
    page_fault_mode = PFM_PROP;
    sd = *_sd;
    page_fault_mode = m;

    if (sd.sd_dpl == 0)
	sd.sd_dpl = 1;
    
    gdt[i] = sd;
    return 0;
}

int
sys_set_ldt (u_int sn, u_int k, u_int16_t sel)
{
    int i = (sel >> 3);

    if (i>=GD_NULLS)
	return -E_INVAL;

    /* FIXME:  check that descriptor i in the GDT is valid */

    asm("lldt %0" :: "m" (sel));

    return 0;
}

int
sys_run_guest (u_int sn, u_int k, tfp_vm86 tf, u_int cr0, u_int handler)
{
    cap c;
    int r;

    if ((r = env_getcap (curenv, k, &c)) < 0)
	return (r);
    if (c.c_isptr || c.c_len || c.c_perm != CL_ALL)
	return (-E_CAP_INSUFF);

    if (! (isreadable_varange ((u_int) tf, sizeof (*tf)))) {
	return -E_FAULT;
    }

    curenv->handler = handler;
    curenv->handler_stack = (u_int)tf+sizeof(struct Trapframe_vm86);

    tf->tf_eflags |= 0x202;
    tf->tf_eflags &= ~(3 << 12);   /* 0 iopl */
    
#ifdef MULTIPLE_GUEST
    /* 3 things (at least) need to be shared when running multiple guests:
       the guest's GDT entries need to be reloaded, and the LDTR register
       needs to be reloaded. Oh and TSS. */

    /* FIXME: pointer to guest GDT:  reparse/restore entries */

    if (ldt_sel) {
	if ((ldt_sel>>3)>=GD_NULLS)
	    return -E_INVAL;
	/* FIXME:  check that descriptor i in the GDT is valid */
	asm volatile ("lldt %0" :: "m" (ldt_sel));
    }

    /* FIXME:  TSS entries */
#endif

    /* I return directly from here because syscalls don't save
       registers, so they wouldn't try to restore any.  */

    if (tf->tf_eflags & (1<<17)) {
	asm volatile ("movl %0, %%esp"::"g" (tf));
	asm volatile ("popal\n"
		      "add $8, %esp\n"	/* skip the protected mode ds/es */
		      "iret");
    } else {
	if (tf->tf_cs >= GD_NULLS*8 ||
	    tf->tf_ss >= GD_NULLS*8 ||
	    (tf->tf_cs & 3) == 0)
	    return -E_INVAL;
	if ((tf->tf_ss & ~3) == 0)
	    /* or SS selector RPL != SS descriptor DPL */
	    /* SS descriptor non writable */
	    /* DS, ES is not data or not readable code seg */
	    /* SS, DS, ES seg descr is marked not present */
	    /* etc... */
	    return -E_FAULT;
	/* ensure RPL of 1 not 0.  I  */
	if ((tf->tf_cs & 3) == 0)
	    tf->tf_cs ++;

	/* NT must be clear for this to stick */
	if (! (cr0 & 8)) {
	    asm volatile ("movl %cr0, %eax\n"
			  "andl $0xfffffff3, %eax\n"
			  "movl %eax, %cr0");
	}

	asm volatile ("movl %0, %%esp\n"
		      "popal\n"
		      "popl %%es\n"
		      "popl %%ds\n"
		      "iret"::"g" (tf));
    }
    return -E_UNSPECIFIED;
}
#endif

#ifdef __ENCAP__
#include <xok/sysinfoP.h>
#endif

