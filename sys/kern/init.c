
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

#include <xok/pmap.h>
#include <xok/sysinfo.h>
#include <xok/mmu.h>
#include <xok/trap.h>
#include <xok/env.h>
#include <machine/pio.h>
#include <xok/capability.h>
#include <xok/sys_proto.h>
#include <xok/syscallno.h>
#include <xok/disk.h>
#include <ubb/xn.h>
#include <xok/pctr.h>
#include <xok/kdebug.h>
#include <xok/cpu.h>
#include <xok/kstrerror.h>
#include <xok/picirq.h>
#include <xok/pkt.h>
#include <xok/scheduler.h>
#include <xok/console.h>
#include <xok/printf.h>
#include <xok/init.h>
#include <xok/idt.h>
#include <xok/ipi.h>
#include <xok/ipc.h>
#ifdef __SMP__
#include <xok/smp.h>
#include <xok/cpu.h>
#include <xok/apic.h>
#endif
#include "initprog.h"
extern char osid[]; /* XXX - put somewhere else */

#if defined(OSKIT_PRESENT)
/* include file to initialize OS-kit device drivers */
#include <xok/xok_net_interface.h>
#endif

/* CPU context, one for each CPU */
struct CPUContext *__cpucxts[NR_CPUS];

/* XXX - hack: I am not sure how best to do this, but gdt and idt should
 * really be on 32 byte boundary for efficient memory access w/r to cache
 * lines. so here I am padding gdt. adjust the size if you see kernel messages
 * saying that this size should be adjusted! */
unsigned char gdt_padding[4] = {0,};

/* Global descriptor table.  Built using:
 *    seg (type, base, limit, dpl)
 */
struct seg_desc gdt[NR_GDTES] =
{
#ifdef __HOST__
  /* The number of NULL segments here must match GD_NULLS in mmu.h */
#define NULL_SEG_4   mmu_seg_null, mmu_seg_null, mmu_seg_null, mmu_seg_null,
#define NULL_SEG_16  NULL_SEG_4  NULL_SEG_4  NULL_SEG_4  NULL_SEG_4
#define NULL_SEG_64  NULL_SEG_16 NULL_SEG_16 NULL_SEG_16 NULL_SEG_16
#define NULL_SEG_256 NULL_SEG_64 NULL_SEG_64 NULL_SEG_64 NULL_SEG_64

  NULL_SEG_256
  NULL_SEG_256
  NULL_SEG_256
  NULL_SEG_256
  NULL_SEG_256
  NULL_SEG_256
#else
  /* 0x0 - unused (always faults--for trapping NULL far pointers) */
  mmu_seg_null,
#endif

  /* 0x8 - kernel code segment */
  [GD_KT >> 3] = seg (STA_X | STA_R, 0x0, 0xffffffff, 0),
  /* 0x10 - kernel data segment */
  [GD_KD >> 3] = seg (STA_W, 0x0, 0xffffffff, 0),
  /* 0x18 - user code segment */
  [GD_UT >> 3] = seg (STA_X | STA_R, 0x0, ULIM - 1, 3),
  /* 0x20 - user data segment */
  [GD_UD >> 3] = seg (STA_W, 0x0, ULIM - 1, 3),
  /* 0x28 - ASH device window */
  [GD_AW >> 3] = mmu_seg_null,
  /* 0x30 - Ethernet transmit buffer */
  [GD_ED0 >> 3] = mmu_seg_null,
  /* 0x38 - test segment descriptor performance */
  [GD_TEST >> 3] = seg (STA_W, USTACKTOP-NBPG, USTACKTOP - 1, 3),
  /* 0x40 - tss */
  [GD_TSS >> 3] = mmu_seg_null,
};
struct pseudo_desc gdt_pd =
{
  0, sizeof (gdt) - 1, (unsigned long) gdt,
};

/* Interrupt descriptor table.  (Must be built at run time because
 * shifted proc addresses can't be represented in relocation records.)
 */
struct gate_desc idt[256] = {{0},};
struct pseudo_desc idt_pd =
{
  0, sizeof (idt) - 1, (unsigned long) idt,
};


int booting_up;

void
random_intr (int irq)
{
  u_int *stack = (&irq) + 11; /* skip irq, pushal registers (8), es, ds */

  printf ("unaccounted IRQ %d on CPU %d:", irq, cpu_id);
  printf (" eip 0x%x;", *stack++);
  printf (" cs = 0x%x;", *stack++ & 0xffff);
  printf (" eflags = 0x%x\n", *stack++);
}

void
random_trap (int trap, int errcode)
{
  tfp tf;

  printf ("just got TRAP %d in env 0x%x on CPU %d", trap,
	  curenv ? curenv->env_id : -1, cpu_id);

  if (trap >= 8 && trap < 32) {
    printf ("; error code = 0x%x\n", errcode);
    tfp_set (tf, errcode, tf_edx);
  }
  else {
    printf ("\n");
    tfp_set (tf, trap, tf_edx);
  }

  printf ("  eip = 0x%x;", tf->tf_eip);
  printf (" cs = 0x%x;", tf->tf_cs & 0xffff);
  printf (" eflags = 0x%x\n", tf->tf_eflags);
  printf ("  esp = 0x%x;", tf->tf_esp);
  printf (" ebp = 0x%x;", tf->tf_ebp);
  printf (" ss = 0x%x;", tf->tf_ss & 0xffff);
  printf (" ds = 0x%x;", tf->tf_ds & 0xffff);
  printf (" es = 0x%x\n", tf->tf_es & 0xffff);

  if (tf->tf_cs == GD_KT && trap == 13) {
    tfp t = &curenv->env_tf;
    struct seg_desc *dp;
    printf ("TF: eip = 0x%x, esp = 0x%x, cs = 0x%x, ss = 0x%x\n",
	    t->tf_eip, t->tf_esp, t->tf_cs, t->tf_ss);
    dp = &gdt[t->tf_cs >> 3];
    printf ("CS:  type: 0x%x, base: 0x%x, lim: 0x%x\n",
	    dp->sd_type, ((dp->sd_base_31_24 << 24)
			  | (dp->sd_base_23_16 << 16) | dp->sd_base_15_0),
	    (dp->sd_lim_19_16 << 16) | dp->sd_lim_15_0);
    dp = &gdt[t->tf_ss >> 3];
    printf ("SS:  type: 0x%x, base: 0x%x, lim: 0x%x\n",
	    dp->sd_type, ((dp->sd_base_31_24 << 24)
			  | (dp->sd_base_23_16 << 16) | dp->sd_base_15_0),
	    (dp->sd_lim_19_16 << 16) | dp->sd_lim_15_0);
  }

  if (tf->tf_cs == GD_KT)
    panic ("That does it (Unhandled trap in kernel).");

  kill_env (curenv);
}
  

static void
check_cache_align()
{
  int need_delay = 0;
  u_int gdt_off = ((u_int) gdt) & 31;

  if ((sizeof(struct Quantum) % 4) != 0)
  {
    printf ("***********************************************************\n");
    printf ("* WARNING: struct Quantum no longer 32 bit aligned         \n");
    printf ("***********************************************************\n");
    need_delay = 1;
  }

  if ((sizeof(struct Env) % 4) != 0)
  {
    printf ("***********************************************************\n");
    printf ("* WARNING: struct Env no longer 32 bit aligned             \n");
    printf ("***********************************************************\n");
    need_delay = 1;
  }

  if ((((u_int)&__envs[0]) % 4) != 0)
  {
    printf ("***********************************************************\n");
    printf ("* WARNING: Env array no longer 32 bit aligned              \n");
    printf ("***********************************************************\n");
    need_delay = 1;
  }

  if (gdt_off != 0)
  {
    printf ("***********************************************************\n");
    printf ("* WARNING: global descriptor table no longer aligned.     *\n"
            "*          Adjust gdt padding to get optimal performance! *\n");
    printf ("***********************************************************\n");
    need_delay = 1;
  }

  if ((PDENO(UADDR) & ~7) != (PDENO(CPUCXT) & ~7) ||
      (PDENO(UADDR) & ~7) != (PDENO(KSTACKTOP-1) & ~7))
  {
    printf ("***********************************************************\n");
    printf ("* WARNING: UADDR, CPUCXT, and KSTACKTOP-1 have different  *\n"
            "*          PDENO. Adjust to get optimal performance!      *\n");
    printf ("***********************************************************\n");
    need_delay = 1;
  }

  if (need_delay)
    delay(1000000);
}


static void
launch_init (void)
{
  int i;
  struct Env *e;
  Pte pte = PG_P | PG_U | PG_W;
  Pte *ptep;
  struct Ppage *pp;
  u_int va = UTEXT, num_completed;
#define _paste(x, y) x ## y
#define paste(x, y) _paste (x, y)
#define initbin paste (INITPROG, _bin)
#define initsize paste (INITPROG, _bin_size)
  extern u_char initbin[];
  extern const u_int initsize;
  int r;

  e = init_env;

  /* Give init the zero length capability with all perms */
  cap_init_zerolength (&e->env_clist[0]);

  set_cap_uarea (e, &e->env_clist[0]);

  assert (cpu_id == 0);
  curenv = e;
#ifdef __SMP__
  curenv->env_cur_cpu = cpu_id;
#endif

  /* Copy program to user memory */
  for (i = 0; i < initsize; va += NBPG) {
    num_completed = 0;
    if ((r = pmap_insert_range (SYS_self_insert_pte_range, 0, &pte, 1, va,
				&num_completed, 0, 0, 0)) < 0)
      panic ("launch_init: could not alloc pages for init prog. "
	     "Errno %d: %s.\n", r, kstrerror(r));
    ptep = env_va2ptep (e, va);
    pp = ppages_get(*ptep >> PGSHIFT);
    if (i == 0) {
      /* pretend we have an a.out header at the start of us... */
      bcopy (&initbin[i], pp2va (pp) + 0x20, NBPG - 0x20);
      i += (NBPG - 0x20);
    }
    else {
      bcopy (&initbin[i], pp2va (pp), NBPG);
      i += NBPG;
    }
  }
  /* Create a user stack */
  num_completed = 0;
  if ((r = pmap_insert_range (SYS_self_insert_pte_range, 0, &pte, 1,
			      USTACKTOP - NBPG, &num_completed, 0, 0, 0)) < 0)
    panic ("launch_init: could not alloc static pages for init prog. "
	   "Errno %d: %s.\n", r, kstrerror(r));

  /* Give init full access to all disks */
  e->env_status = ENV_OK;
  e->env_tf.tf_eip = UTEXT + 0x20; /* 0x20 is to skip initial a.out header */
  if (!e->env_u)
    panic ("launch_init: no U structure after mapping stack.\n");
  bzero (e->env_u, sizeof (*e->env_u));
  e->env_u->u_status = U_RUN;
  e->env_u->pid = 1;
  e->env_u->parent = 0;
  e->env_u->u_chld_state_chng = 0;
  e->env_u->u_in_pfault = 0;
  e->env_u->u_donate = -1;
  e->env_u->u_revoked_pages = 0;

  r = sys_quantum_alloc (SYS_quantum_alloc, 0, -1, 0, e->env_id);

  if (r < 0)
    panic ("launch_init: could not get initial quantum. "
	   "Errno %d: %s.\n", r, kstrerror(r));
}

void osid_init(void) {
  int n = MAXOSID;
  register char *d = SYSINFO_GET(si_osid);
  register const char *s = osid;

  while (--n > 0) {
    if ((*d++ = *s++) == 0) {
      return;
    }
  } 
  if (n == 0) *d = (char)0;
}



/* initializes CPU context */
void
cpucxt_init (void)
{
  __cpucxts[get_cpu_id()]->_cpu_id = get_cpu_id ();
#ifdef __SMP__
  __cpucxts[get_cpu_id()]->_apic_id = get_apic_id ();
#endif
  MP_SPINLOCK_INIT(&(__cpucxts[get_cpu_id()]->_ipi_spinlock));
  __cpucxts[get_cpu_id()]->_in_ipih = 0;
  __cpucxts[get_cpu_id()]->_ipi_pending = 0;
  bzero(__cpucxts[get_cpu_id()]->_ipiq, IPIQ_SIZE*sizeof(ipimsg_t));
  __cpucxts[get_cpu_id()]->_last_ticket = 0;
}


/*
 * A few notes on SMP startup:
 *
 * 1) we must first detect smp, and figure out how many APs there are
 * 2) in ppage_init, we allocate temporary stacks for each AP
 * 3) we boot APs, get them to i386_init_secondary. They initialize
 *    themselves, but before going to scheduler, wait til everyone else
 *    is done.
 * 4) we make sure ALL of them are initialized, then finish our
 *    initialization stuff, this is so that in late initialization
 *    process we may actually consider APs functional, just not running
 *    schedulers yet.
 * 5) right before we head to scheduler, we do smp_commence() to release
 *    all APs and have them go into their schedulers as well.
 */

void
i386_init (void)
{
  extern void exo_ed_init (void);
  extern void pci_init (void);
  extern int cpuid_vers, cpuid_features;
  extern void dpf_init_refs ();
  extern int bc_init (void);
  extern void pxn_init ();
  extern void give_away_crt ();
  extern int partition_init ();
  extern void micropart_init ();
  extern void syscall_init ();
  extern void mp_locks_init ();
  extern void pktring_init ();
  booting_up = 1;

  cninit ();

  printf ("%s\n", osid);
  printf ("CPU family %d%s\n", (cpuid_vers >> 8) & 0xf,
	  (cpuid_features & 8) ? " (using PSE)" : "");

#ifdef __SMP__
  /* first scan for smp mode */
  /* KEY: this MUST be done before ppage_init() so that in 
   * ppage_init() we can allocate a nice chunk of kernel 
   * low memory to bootstrap the APs. 
   * we also manually map the APIC regions */
  smp_scan ();
#endif

  ppage_init ();
  give_away_crt ();

  idt_init ();
  pic_init ();

  env_init ();
  cpucxt_init (); 

#ifdef __SMP__
  /* locks need cpucxt info, so before cpucxt_init is not helpful */
  mp_locks_init (); 
#endif

  pctr_init ();
  clock_init (0);
  mhz_init ();  // must be done before APIC is setup

#ifdef __SMP__
  smp_boot (); // must come after env_init, so the initial pt is all set up
#endif

#ifdef __SMP__
  {
    extern void rand_init();
    rand_init();
  }
#endif

  sched_init ();
  syscall_init ();

  kbd_init ();
  xoknet_init ();
  pktring_init ();
  pci_init ();
  dpf_init_refs ();
  exo_ed_init ();

  bc_init ();
  pxn_init ();

#if defined(OSKIT_PRESENT)
  /*set up the OS-kit device drivers */
  oskit_init();
#endif

#if HAVE_DISK

  /* must be done after bc and init are setup, but before xn */

  /* if we didn't find any partitions, don't give 
     permission to access anything out (si_disks still
     refers to the physical disks) */

  if (partition_init () > 0) {
    disk_giveaway();
  }
#ifdef XN
  sys_xn_init();
#endif
  micropart_init ();
#endif /* HAVE_DISK */

  osid_init();
  printf ("%s\n", osid);

#ifdef __SMP__
  /* switch to APIC LAST */

  if (smp_mode) {
    /* make sure we have an IO APIC that we can use, and 
     * move interrupt control from 8259A to IOAPIC if we do.
     * this should be done last to allow all useful IRQs to be 
     * registered by the driver codes */
    ioapic_init ();
  }
#endif

#ifdef KDEBUG
  kdebug_init ();
#endif
  booting_up = 0;

#ifdef __SMP__
  smp_init_done();
  smp_commence ();
  /* at this point system is a go... all APs are running at full speed */
#endif
  printf("SMP commenced\n");

  check_cache_align();
  
  launch_init ();
  sched_runnext ();

  panic ("init.c(i386_init): sched_runnext returned");
}

#ifdef __SMP__
  
void
i386_init_secondary (void)
{
  extern void env_init_secondary ();
  extern void ppage_init_secondary ();
  extern void i386_start_cpu ();
  
  smp_callin ();  /* tell BSP we are up, so it can do some stuff for us */

  ppage_init_secondary (); 
  env_init_secondary (); 
  cpucxt_init ();

  pctr_init();
  mhz_init();

  /* enable local APIC, must be done AFTER env_init_secondary */
  localapic_enable ();

  asm volatile("movl %0,%%esp\n" :: "i" (KSTACKTOP-sizeof(struct Trapframe)));

  smp_init_done();
  smp_waitfor_commence();

  sched_runnext ();
}

#endif

#ifdef __HOST__
/*
  Call this when the exception needs to be returned to user space.
  Expects to have the stack same as when it first traps.
*/
void
fault_trampoline (int trap, int errcode)
{
    int m;
    u_int handler;
    int framesize = sizeof(struct Trapframe_vm86);
    tfp_vm86 tf = (tfp_vm86)((u_int)&errcode + 4);	/* true tf */
    tfp_vm86 u_tf;					/* tf for monitor */

    if (curenv->handler) {
	handler = curenv->handler;
    } else {
	handler = UIDT_VECT_TO_ADDR(trap);
    }
    if (curenv->handler_stack) {
	u_tf = (tfp_vm86)(curenv->handler_stack - sizeof(struct Trapframe_vm86));
    } else {
	u_tf = (tfp_vm86)(USTACKTOP - sizeof(struct Trapframe_vm86));
    }

    if (! (tf->tf_eflags & (1<<17)))
	framesize -= 4*4;   /* not VM86 so trap frame is truncated */

    /* Give the handler the original trap frame... */
    m = page_fault_mode;
    page_fault_mode = PFM_PROP;
    memcpy(u_tf, tf, framesize);
    *(((u_int*)u_tf)-1) = errcode;
    *(((u_int*)u_tf)-2) = trap;
    asm volatile ("mov %%cr2, %0":"=r" (u_tf->tf_oesp));
    page_fault_mode = m;
    
    /* ...but modify the kernel's trap frame so we can return sanely
       to a user-level handler. */
    tf->tf_esp = tf->tf_ebp = ((u_int)u_tf) - 12; /* trapno, errcode, and ?? */
    tf->tf_eip = handler;
    tf->tf_eflags = 0x202;
    tf->tf_ds  = GD_UD|3;
    tf->tf_es  = GD_UD|3;
    tf->tf_ss  = GD_UD|3;
    tf->tf_cs  = GD_UT|3;
    
    env_pop_tf ((tfp)tf);
}

void
general_protection_fault (int trap, int errcode)
{
  u_int esp, eflags;
  tfp tf = (tfp)(&errcode+1);

  /* Tricky.  May err if just exited VM86 but still with a
     cs==GD_UT|3, by chance (cs hasn't been reloaded with protected
     mode selector yet).  Would get directed to uidt instead of the
     monitor.  I suppose a good monitor would have such a uidt
     installed anyway, so maybe it doesn't matter. */
  /* or maybe only VM86 should go to the handler; everything else
     via uidt?  Be consistent, and let the monitor sort it out? */
  if (tf->tf_cs != (GD_UT|3) && curenv->handler) {
      asm volatile ("movl %0, %%eax\n"
		    "leave\n"
		    "jmp %%eax" :: "g" ((u_int)&fault_trampoline));
  }

  /* it's normal UIDT.  Let's try to get it back to user space. */
  eflags = tf->tf_eflags;
  asm volatile("mov %%esp, %0" : "=g" (esp));		/* save kernel's esp */
  asm volatile("mov %0, %%esp" :: "g" (tf->tf_esp));	/* go to user's tf... */
  asm volatile("pushl %0" :: "g" (tf->tf_eflags));	/* and rewrite it. */
  asm volatile("pushl %0" :: "g" (GD_UT|3));
  asm volatile("pushl %0" :: "g" (tf->tf_eip));
  asm volatile("pushl %0" :: "g" (errcode));
  asm volatile("mov %0, %%esp" :: "g" (esp));	/* return to kernel's */
  tf->tf_esp -= 4*4;				/* keep stack we just made */
  tf->tf_eip = UIDT_VECT_TO_ADDR(0x0d);
}
#endif

#ifdef __ENCAP__
#include <xok/pmapP.h>
#include <xok/sysinfoP.h>
#endif

