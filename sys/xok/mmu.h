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


#ifndef _XOK_MMU_H_
#define _XOK_MMU_H_

#include <machine/param.h>

/* Page tables (selected by VA[31:22] and indexed by VA[21:12]) */
#define NLPG (1<<(PGSHIFT-2))   /* Number of 32-bit longwords per page */
#define PGMASK (NBPG - 1)       /* Mask for page offset.  Terrible name! */
/* Page number of virtual page in the virtual page table. */
#define PGNO(va) ((u_int) (va) >> PGSHIFT)
/* Index of PTE for VA within the corresponding page table */
#define PTENO(va) (((u_int) (va) >> PGSHIFT) & 0x3ff)
/* Round up to a page */
#define PGROUNDUP(va) (((va) + PGMASK) & ~PGMASK)
/* Round down to a page */
#define PGROUNDDOWN(va) ((va) & ~PGMASK)
/* Page directories (indexed by VA[31:22]) */
#define PDSHIFT 22             /* LOG2(NBPD) */
#define NBPD (1 << PDSHIFT)    /* bytes/page dir */
#define PDMASK (NBPD-1)        /* byte offset into region mapped by
				  a page table */
#define PDENO(va) ((u_int) (va) >> PDSHIFT)
/* Round up  */
#define PDROUNDUP(va) (((va) + PDMASK) & ~PDMASK)
/* Round down */
#define PDROUNDDOWN(va) ((va) & ~PDMASK)

/* At IOPHYSMEM (640K) there is a 384K hole for I/O.  From the kernel,
 * IOPHYSMEM can be addressed at KERNBASE + IOPHYSMEM.  The hole ends
 * at physical address EXTPHYSMEM. */
#define IOPHYSMEM 0xa0000
#define EXTPHYSMEM 0x100000


/*
 * Virtual memory map:
 *
 *    4 Gig -------->  +------------------------------+
 *		       |			      |
 *		      ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *		      ~|~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|~
 *		       |			      |
 *		       |  Physical Memory	      |
 *		       |			      |
 *    KERNBASE ----->  +------------------------------+
 *		       |			      |
 *		       |  Kernel Virtual Page Table   |
 *		       |			      |
 *    VPT ---------->  +------------------------------+
 *		       |			      |
 *		       |  Global VM for ASH's         |
 *    ASHVM/           |			      |
 *    KSTACKTOP -----> +------------------------------+
 *		       |  Kernel Stack/Ash Dev Mem    |
 *    ASHDVM --------> | ---------------------------- |
 *                     |  Local APIC Memory Map       |
 *    LAPIC  --------> | ---------------------------- |
 *                     |  I/O   APIC Memory Map       |
 *    IOAPIC --------> | ---------------------------- |
 *                     |                              |
 *                     |  Rd-only CPU context struct  |
 *                     |                              |
 *    CPUCXT  ------>  +------------------------------+
 *		       |  Invalid memory   	      |
 *    ULIM ----------> +------------------------------+
 *                     |                              |
 *       	       |  Uenv aka uarea, udot struct |
 *    URO              |                              |
 *    UADDR -------->  +------------------------------+
 *		       |			      |
 *		       |  User (read-only) VPT	      |
 *		       |			      |
 *    UVPT --------->  +------------------------------+
 *		       |			      |
 *		       |  Read-only Ppage structs     |
 *		       |			      |
 *    UPPAGES ------>  +------------------------------+
 *		       |			      |
 *		       |  Read-only Sysinfo/Env strct |
 *    SYSINFO/         |                	      |
 *    UENVS  ------->  +------------------------------+
 *                     |                              |
 *                     |  Read-only Buffer Cache      |
 *                     |                              |
 *    UBC ---------->  +------------------------------+
 *                     |                              |
 *                     |  Read-only XN registry       |
 *                     |                              |
 *    UXN -----------> +------------------------------+
 *                     |                              |
 *                     |  Read-only Disk freemap      |
 *    UTOP             |                              |
 *    UXNMAP --------> +------------------------------+
 *        	       |    user interrupt table      |
 *    UIDT  -------->  +------------------------------+
 *        	       |  unmapped page to prot uidt  |
 *    USTACKTOP ---->  +------------------------------+
 *                     |			      |
 *		       |  Application VM	      |
 *		       |			      |
 *		      ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *		      ~|~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|~
 *    UTEXT ------->   +------------------------------+
 *		       |			      |
 *		       |  Locally accesible ASH VM    |
 *    UASH/	       |			      |
 *    0 ------------>  +------------------------------+
 */


/* All physical memory mapped at this address */
/* KERNBASE defined in sys/machine/param.h */

 /* Virtual page table.  Last entry of all PDEs contains a pointer to
 * the PD itself, thereby turning the PD into a page table which
 * maps all PTEs over the last 4 Megs of the virtual address space */
#define VPT (KERNBASE - NBPD)
/* Each environment has 4 Megs of global virtual address space for
 * ASHs above this virtual address. */
#define NASHPD 128
#define ASHVM (VPT - NASHPD * NBPD)

/* Top of the kernel stack (which, to catch overflow errors, isn't in
 * pseudo-physically addressed above kernbase) */
#define KSTACKTOP ASHVM
/* leave one meg for all kernel stack */
#define KSTACKGAP (2<<19)
#define KSTKSIZE (8 * NBPG)   		/* size of a kernel stack */

#define ASHDVMTOP (KSTACKTOP - KSTACKGAP)
/* 640K of device memory mapped here for ashes, allocate 1meg VA */
#define ASHDVM (ASHDVMTOP - (2<<19))  

/* 1meg local APIC space, mapped from higher physical address space */
#define LAPIC (ASHDVM - (2<<19)) 
/* 1meg IO APIC space, mapped from higher physical address space */
#define IOAPIC (LAPIC - (2<<19)) 

/* CPU context */
#define CPUCXT (IOAPIC - NBPD)

/*
 * ULIM
 *
 * Anything below ULIM is okay to write in kernel mode at the user's request.
 * (i.e. something illegal will cause a fault, propagated to user). Also, user
 * GDT entry specifies ULIM as an upper limit.
 */

#define ULIM (CPUCXT - NBPD) 

/*
 * UADDRS and UADDR: first 2 NBPD under ULIM. Kernel controls insertion and
 * deletion of pages into these 2 NBPD. For mapping Uenv structures. Users can
 * write to Uenv mapped here, but cannot change their mappings. 
 * 
 * Should keep these two addresses close to CPUCXT and KSTACKTOP for best
 * cache performance when reloading them!
 */

/* location in user space for Uenv structures */
#define UADDRS_SIZE NBPD
#define UADDRS (ULIM - UADDRS_SIZE)

/* kernel mapped in Uenv structure */
#define UADDR (UADDRS - NBPD)

/*
 * URO
 *
 * User read-only mappings! Anything below here til UTOP are readonly to user.
 * They are global pages mapped in at env allocation time.
 */

#define URO UADDR

/* Same as VPT but read-only for users */
#define UVPT (URO - NBPD)
/* Read-only copies of all ppage structures */
#define UPPAGES (UVPT - NBPD)
/* Read only copy of the global sysinfo structure */
#define SYSINFO (UPPAGES - NBPD)
#define SYSINFO_SIZE (10 * NBPG)
/* Read-only copies of all env structures (XXX) */
#define UENVS (SYSINFO + SYSINFO_SIZE)
/* Read-only copy of the buffer cache */
#define UBC (SYSINFO - NBPD)
/* Read-only copy of the XN registry */
#define UXNMAP (UBC - NBPD)
#define UXNMAP_SIZE (16 * NBPG)
/* Read-only XN disk free map */
#define UXN (UXNMAP + UXNMAP_SIZE)
#define UXN_SIZE (25 * NBPG)

/*
 * UTOP
 *
 * Top of user VM. User can manipulate VA from UTOP-1 and down!
 */

#define UTOP UXNMAP

#define UIDT_STUB_SIZE 32       /* size of each stub in bytes */
#define UIDT_NUM_STUBS 256      /* 32x256 = 8192, ie. 2 pages are used */
#define UIDT (UTOP - (UIDT_STUB_SIZE*UIDT_NUM_STUBS)) 
 				/* Base of the "user IDT" */
#define UIDT_VECT_TO_ADDR(vect_num) ((UIDT) + (UIDT_STUB_SIZE)*(vect_num))

/* USTACKTOP is the top of the normal user stack. Leaves an extra page 
 * between itself and UIDT to give uidt table some protection from stack
 * underflow */
#define USTACKTOP (UIDT - NBPG)
#define USTKSIZE (16 * NBPG)   		/* size of an user stack */

#define UTEXT (2*NBPD)
/* Fixed location in ash space for Uenv structure */
#define ASH_UADDR NBPG

/* Number of page tables for mapping physical memory at KERNBASE.
 * (each PT contains 1K PTE's, for a total of 128 Megabytes mapped) */
#define NPPT ((-KERNBASE)>>PDSHIFT)


/* Kernel virtual address at which physical address pa is mapped */
#define ptov(pa) ((void *) ((u_int) (pa) + KERNBASE))

#ifdef KERNEL

/* Values of page_fault_mode */
#define PFM_NONE 0x0     /* No page faults expected.  Must be a bug */
#define PFM_PROP 0x1     /* Propagate the current page fault to the user */
#define PFM_KILL 0x2     /* User was not allowed to fault.  Kill him. */
#define PFM_VCOPY 0x3    /* In a bcopy, handler should zero %ecx */

#endif /* KERNEL */



#ifndef __ASSEMBLER__
#ifdef KERNEL

#include <xok/types.h>


/* Trap page faults in a particular line of code. */
#define pfm(pfm, code...)			\
{						\
  int __m_m = page_fault_mode;			\
  page_fault_mode = pfm;			\
  code;						\
  page_fault_mode = __m_m;			\
}

/* An address is a kernel address if and only if it resides above
 * ULIM.  ULIM itself is guaranteed never to contain a valid page.
 * The CR0_WP bit guarantees we will fault when trying to write a
 * read-only user page, even in kernel mode.  These macros take a user
 * supplied address and turn it into something that will cause a fault
 * if it is a kernel address.  "novbcopy" copies regions which are not
 * overlapping, and so always copies forwards.
 */
#define trup(_p)  /* Translate User Pointer */			\
({								\
  register typeof ((_p)) __m_p = (_p);				\
  ash_va							\
    ? (typeof (_p)) (ash_va + ((u_int) __m_p > NBPD ?		\
			       NBPD : (u_int) __m_p))		\
    : (u_int) __m_p > ULIM ? (typeof (_p)) ULIM : __m_p;	\
})
/*
 * Non-overlapping bcopy.  Always copies forwards.
 */
#define novbcopy(src, dst, len)					\
do {								\
  if (__builtin_constant_p (len) && ! (len & 0x3))		\
    asm volatile ("cld\n"					\
		  "\tshrl $2,%%ecx\n"				\
		  "\trep\n"					\
		  "\t movsl\n"					\
		  :: "S" (src), "D" (dst), "c" (len)		\
		  : "ecx", "esi", "edi", "cc", "memory");	\
  else								\
    asm volatile ("cld\n"					\
		  "\tmovl %2,%%ecx\n"				\
		  "\tshrl $2,%%ecx\n"				\
		  "\trep\n"					\
		  "\t movsl\n"					\
		  "\tmovl %2,%%ecx\n"				\
		  "\tandl $3,%%ecx\n"				\
		  "\trep\n"					\
		  "\t movsb"					\
		  :: "&S" (src), "&D" (dst), "&g" (len)		\
		  : "%ecx", "%esi", "%edi", "cc", "memory");	\
} while (0)

     /* XXX -- gcc and novbcopy don't get along very well. The
	compiler doesn't seem to notice that ecx is being clobbered
	in the asm and tries to load things using it. to get around
	this we use bcopy below. */

#if 1
#undef novbcopy
#define novbcopy bcopy
#endif

#define copyin(s, d, l) pfm (PFM_PROP, novbcopy (trup (s), d, l))
#define copyout(s, d, l) pfm (PFM_PROP, novbcopy (s, trup (d), l))

extern u_long nppage;

/* translates from virtual address to physical address */
#define kva2pa(kva)						\
({								\
  u_long a = (u_long) (kva);					\
  if ((u_long) a < KERNBASE)					\
    panic (__FUNCTION__ ":%d: kva2pa called with invalid kva",	\
	   __LINE__);						\
  a - KERNBASE;							\
})

/* translates from physical address to virtual address */
#define pa2kva(pa)						\
({								\
  u_long ppn = (u_long) (pa) >> PGSHIFT;			\
  if (ppn >= nppage)						\
    panic (__FUNCTION__ ":%d: pa2kva called with invalid pa",	\
	   __LINE__);						\
  (pa) + KERNBASE;						\
})

#endif /* KERNEL */

/* translates physical page # to pte with setting flags */
#define ppnf2pte(ppn, flags) (((ppn) << PGSHIFT) | ((flags) & PGMASK))

#ifndef KERNEL

/* translates virtual address to physical page number */
#define va2ppn(va) (PGNO(vpt[PGNO((va))]))

/* translates virtual address to page table entry with setting flags */
#define vaf2pte(va, flags) ppnf2pte(va2ppn((va)), flags)

/* translates virtual address to page table entry */
#define va2pte(va) (vpt[PGNO((va))])

/* translates virtual page to virtual address */
#define vp2va(vp) ((vp) << PGSHIFT)

/* translates virtual address to virtual page - same as PGNO */
#define va2vp(va) ((va) >> PGSHIFT)

#endif

/* checks if va is mapped */
#define isvamapped(va) \
     (((vpd[PDENO((va))] & (PG_P | PG_U)) == (PG_P | PG_U)) && \
      ((vpt[PGNO((va))] & (PG_P | PG_U)) == (PG_P | PG_U)))

/* checks if va is readable */
#define isvareadable(va) isvamapped(va)

/* checks if va is writable */
#define isvawriteable(va) \
     (((vpd[PDENO((va))] & (PG_P | PG_U | PG_W)) == (PG_P | PG_U | PG_W)) && \
      ((vpt[PGNO((va))] & (PG_P | PG_U | PG_W)) == (PG_P | PG_U | PG_W)))



typedef unsigned int Pte;
typedef unsigned int Pde;

/* The page directory entry corresponding to the virtual address range
 * from VPT to (VPT+NBPD) points to the page directory itself
 * (treating it as a page table as well as a page directory).  One
 * result of treating the page directory as a page table is that all
 * PTE's can be accessed through a "virtual page table" at virtual
 * address VPT (to which vpt is set in locore.S).  A second
 * consequence is that the contents of the current page directory will
 * always be available at virtual address (VPT+(VPT>>PGSHIFT)) to
 * which vpd is set in locore.S.
 */
extern Pte vpt[];     /* VA of "virtual page table" */
extern Pde vpd[];     /* VA of current page directory */

#endif /* !__ASSEMBLER__ */




/* Page Table/Directory Entry flags */
#define PG_P 0x1               /* Present */
#define PG_W 0x2               /* Writeable */
#define PG_U 0x4               /* User */
#define PG_PWT 0x8             /* Write-Through */
#define PG_PCD 0x10            /* Cache-Disable */
#define PG_A 0x20              /* Accessed */
#define PG_D 0x40              /* Dirty */
#define PG_PS 0x80             /* Page Size */
#define PG_MBZ 0x180           /* Bits must be zero */

/* Control Register flags */
#define CR0_PE 0x1             /* Protection Enable */
#define CR0_MP 0x2             /* Monitor coProcessor */
#define CR0_EM 0x4             /* Emulation */
#define CR0_TS 0x8             /* Task Switched */
#define CR0_ET 0x10            /* Extension Type */
#define CR0_NE 0x20            /* Numeric Errror */
#define CR0_WP 0x10000         /* Write Protect */
#define CR0_AM 0x40000         /* Alignment Mask */
#define CR0_NW 0x20000000      /* Not Writethrough */
#define CR0_CD 0x40000000      /* Cache Disable */
#define CR0_PG 0x80000000      /* Paging */

#define CR4_PCE 0x100          /* Performance counter enable */
#define CR4_MCE 0x40           /* Machine Check Enable */
#define CR4_PSE 0x10           /* Page Size Extensions */
#define CR4_DE  0x08           /* Debugging Extensions */
#define CR4_TSD 0x04           /* Time Stamp Disable */
#define CR4_PVI 0x02           /* Protected-Mode Virtual Interrupts */
#define CR4_VME 0x01           /* V86 Mode Extensions */

/* Eflags register */
#define FL_CF 0x1              /* Carry Flag */
#define FL_PF 0x4              /* Parity Flag */
#define FL_AF 0x10             /* Auxiliary carry Flag */
#define FL_ZF 0x40             /* Zero Flag */
#define FL_SF 0x80             /* Sign Flag */
#define FL_TF 0x100            /* Trap Flag */
#define FL_IF 0x200            /* Interrupt Flag */
#define FL_DF 0x400            /* Direction Flag */
#define FL_OF 0x800            /* Overflow Flag */
#define FL_IOPL0 0x1000        /* Low bit of I/O Privilege Level */
#define FL_IOPL1 0x2000        /* High bit of I/O Privilege Level */
#define FL_NT 0x4000           /* Nested Task */
#define FL_RF 0x10000          /* Resume Flag */
#define FL_VM 0x20000          /* Virtual 8086 mode */
#define FL_AC 0x40000          /* Alignment Check */
#define FL_VIF 0x80000         /* Virtual Interrupt Flag */
#define FL_VIP 0x100000        /* Virtual Interrupt Pending */
#define FL_ID 0x200000         /* ID flag */

/* Page fault error codes */
#define FEC_PR 0x1             /* Page fault caused by protection violation */
#define FEC_WR 0x2             /* Page fault caused by a write */
#define FEC_U  0x4             /* Page fault occured while in user mode */


#ifndef __ASSEMBLER__
#ifdef KERNEL

/* checks virtual address range for readability or writeability */
static inline int isreadable_varange (u_int va, u_int sz) 
{
   if ((! (isvareadable(va))) || (! (isvareadable((va+sz-1))))) {
      return (0);
   }
   while (sz > NBPG) {
      va += NBPG;
      sz -= NBPG;
      if (! (isvareadable(va))) {
         return (0);
      }
   }
   return (1);
}

/* checks virtual address range for readability or writeability */
static inline int iswriteable_varange (u_int va, u_int sz) {
   if ((! (isvawriteable(va))) || (! (isvawriteable((va+sz-1))))) {
      return (0);
   }
   while (sz > NBPG) {
      va += NBPG;
      sz -= NBPG;
      if (! (isvawriteable(va))) {
         return (0);
      }
   }
   return (1);
}

#endif

static inline int
mmu_has_hardware_flags(Pte pte)
{
  if (pte & (PG_P | PG_W | PG_U | PG_PWT | PG_PCD | PG_A | PG_D | PG_PS |
	     0x100))
    return 1;
  return 0;
}

/* Segment Descriptors */
struct seg_desc {
  unsigned sd_lim_15_0 : 16;   /* low bits of segment limit */
  unsigned sd_base_15_0 : 16;  /* low bits of segment base address */
  unsigned sd_base_23_16 : 8;  /* middle bits of segment base */
  unsigned sd_type : 4;        /* segment type (see below) */
  unsigned sd_s : 1;           /* 0 = system, 1 = application */
  unsigned sd_dpl : 2;         /* Descriptor Privilege Level */
  unsigned sd_p : 1;           /* Present */
  unsigned sd_lim_19_16 : 4;   /* High bits of segment limit */
  unsigned sd_avl : 1;         /* Unused (available for software use) */
  unsigned sd_rsv1 : 1;        /* reserved */
  unsigned sd_db : 1;          /* 0 = 16-bit segment, 1 = 32-bit segment */
  unsigned sd_g : 1;           /* Granularity: limit scaled by 4K when set */
  unsigned sd_base_31_24 : 8;  /* Hight bits of base */
};
/* Null segment */
#define mmu_seg_null (struct seg_desc) {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
/* Segment that is loadable but faults when used */
#define mmu_seg_fault (struct seg_desc) {0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0}
/* Normal segment */
#define seg(type, base, lim, dpl) (struct seg_desc)			\
{ ((lim) >> 12) & 0xffff, (base) & 0xffff, ((base) >> 16) & 0xff,	\
    type, 1, dpl, 1, (unsigned) (unsigned) (lim) >> 28, 0, 0, 1, 1,	\
    (unsigned) (base) >> 24 }
#define smallseg(type, base, lim, dpl) (struct seg_desc)		\
{ (lim) & 0xffff, (base) & 0xffff, ((base) >> 16) & 0xff,		\
    type, 1, dpl, 1, (unsigned) (unsigned) (lim) >> 16, 0, 0, 1, 0,	\
    (unsigned) (base) >> 24 }

#ifdef KERNEL

/* Task state segment format (as described by the Pentium architecture
 * book). */
struct Ts {
  u_int ts_link;         /* Old ts selector */
  u_int ts_esp0;         /* Stack pointers and segment selectors */
  u_short ts_ss0;        /*   after an increase in privilege level */
  u_int : 0;
  u_int ts_esp1;
  u_short ts_ss1;
  u_int : 0;
  u_int ts_esp2;
  u_short ts_ss2;
  u_int : 0;
  u_int ts_cr3;          /* Page directory base */
  u_int ts_eip;          /* Saved state from last task switch */
  u_int ts_eflags;
  u_int ts_eax;          /* More saved state (registers) */
  u_int ts_ecx;
  u_int ts_edx;
  u_int ts_ebx;
  u_int ts_esp;
  u_int ts_ebp;
  u_int ts_esi;
  u_int ts_edi;
  u_short ts_es;         /* Even more saved state (segment selectors) */
  u_int : 0;
  u_short ts_cs;
  u_int : 0;
  u_short ts_ss;
  u_int : 0;
  u_short ts_ds;
  u_int : 0;
  u_short ts_fs;
  u_int : 0;
  u_short ts_gs;
  u_int : 0;
  u_short ts_ldt;
  u_int : 0;
  u_short ts_iomb;       /* I/O map base address */
  u_short ts_t;          /* Trap on task switch */
};

#define settss(sd, base)					\
{								\
  (sd).sd_lim_15_0 = (sizeof (struct Ts) - 1) & 0xffff;		\
  (sd).sd_base_15_0 = (u_long) (base) & 0xffff;			\
  (sd).sd_base_23_16 = ((u_long) (base) >> 16) & 0xff;		\
  (sd).sd_type = STS_T32A;					\
  (sd).sd_s = 0;						\
  (sd).sd_dpl = 0;						\
  (sd).sd_p = 1;						\
  (sd).sd_lim_19_16 = ((sizeof (struct Ts) - 1) >> 16) & 0xf;	\
  (sd).sd_avl = 0;						\
  (sd).sd_rsv1 = 0;						\
  (sd).sd_db = 0;              /* Required for TSS? */		\
  (sd).sd_g = 0;						\
  (sd).sd_base_31_24 = (u_long) (base) >> 24;			\
}

/* Gate descriptors are slightly different */
struct gate_desc {
  unsigned gd_off_15_0 : 16;   /* low 16 bits of offset in segment */
  unsigned gd_ss : 16;         /* segment selector */
  unsigned gd_args : 5;        /* # args, 0 for interrupt/trap gates */
  unsigned gd_rsv1 : 3;        /* reserved (should be zero I guess) */
  unsigned gd_type :4;         /* type (STS_{TG,IG32,TG32}) */
  unsigned gd_s : 1;           /* must be 0 (system) */
  unsigned gd_dpl : 2;         /* descriptor (meaning new) privilege level */
  unsigned gd_p : 1;           /* Present */
  unsigned gd_off_31_16 : 16;  /* high bits of offset in segment */
};
/* Normal gate descriptor */
#define setgate(gate, istrap, ss, off, dpl)		\
{							\
  (gate).gd_off_15_0 = (u_long) (off) & 0xffff;		\
  (gate).gd_ss = (ss);					\
  (gate).gd_args = 0;					\
  (gate).gd_rsv1 = 0;					\
  (gate).gd_type = (istrap) ? STS_TG32 : STS_IG32;	\
  (gate).gd_s = 0;					\
  (gate).gd_dpl = dpl;					\
  (gate).gd_p = 1;					\
  (gate).gd_off_31_16 = (u_long) (off) >> 16;		\
}

/* Call gate descriptor */
#define setcallgate(gate, ss, off, dpl)                 \
{							\
  (gate).gd_off_15_0 = (u_long) (off) & 0xffff;		\
  (gate).gd_ss = (ss);					\
  (gate).gd_args = 0;					\
  (gate).gd_rsv1 = 0;					\
  (gate).gd_type = STS_CG32;                            \
  (gate).gd_s = 0;					\
  (gate).gd_dpl = dpl;					\
  (gate).gd_p = 1;					\
  (gate).gd_off_31_16 = (u_long) (off) >> 16;		\
}

/* Pseudo-descriptors used for LGDT, LLDT and LIDT instructions */
struct pseudo_desc {
  u_short pd__garbage;         /* LGDT supposed to be from address 4N+2 */
  u_short pd_lim;              /* Limit */
  u_long pd_base __attribute__ ((packed));       /* Base address */
};
#define PD_ADDR(desc) (&(desc).pd_lim)

#endif /* KERNEL */

#else /* __ASSEMBLER__ */

#ifdef KERNEL
/* Pseudo descriptors are supposed to be at an odd 16-bit word, so the
 * pseudo_desc structure is padded with two bytes of garbage. */
#define PD_ADDR(desc) ((desc) + 2)
#endif /* KERNEL */

#endif /* __ASSEMBLER__ */

/* Application segment type bits */
#define STA_X 0x8              /* Executable segment */
#define STA_E 0x4              /* Expand down (non-executable segments) */
#define STA_C 0x4              /* Conforming code segment (executable only) */
#define STA_W 0x2              /* Writeable (non-executable segments) */
#define STA_R 0x2              /* Readable (executable segments) */
#define STA_A 0x1              /* Accessed */

/* System segment type bits */
#define STS_T16A 0x1           /* Available 16-bit TSS */
#define STS_LDT 0x2            /* Local Descriptor Table */
#define STS_T16B 0x3           /* Busy 16-bit TSS */
#define STS_CG16 0x4           /* 16-bit Call Gate */
#define STS_TG 0x5             /* Task Gate / Coum Transmitions */
#define STS_IG16 0x6           /* 16-bit Interrupt Gate */
#define STS_TG16 0x6           /* 16-bit Trap Gate */
#define STS_T32A 0x9           /* Available 32-bit TSS */
#define STS_T32B 0xb           /* Busy 32-bit TSS */
#define STS_CG32 0xc           /* 32-bit Call Gate */
#define STS_IG32 0xe           /* 32-bit Interrupt Gate */
#define STS_TG32 0xf           /* 32-bit Task Gate */

/* Global descriptor numbers
 *
 * NOTE: when adding new GDTE, add *BEFORE* GD_TSS, change GD_TSS, then update
 * NR_GDTES 
 */
#ifdef __HOST__
#define GD_NULLS  0x600        /* segments for guest OS; must match init.c */
#else
#define GD_NULLS  1            /* one required by hardware; must match init.c */
#endif

#define GD_UNUSED 0x0	       /* unused */
#define GD_KT   (0x00+GD_NULLS*8) /* kernel text */
#define GD_KD   (0x08+GD_NULLS*8) /* kernel data */
#define GD_UT   (0x10+GD_NULLS*8) /* user text */
#define GD_UD   (0x18+GD_NULLS*8) /* user data */
#define GD_AW   (0x20+GD_NULLS*8) /* ASH device window */
#define GD_ED0  (0x28+GD_NULLS*8) /* Transmit buffer for ethernet */
#define GD_TEST (0x30+GD_NULLS*8) /* guest text */
#define GD_TSS  (0x38+GD_NULLS*8) /* Task segment selector for boot CPU */

#define NR_GDTES (GD_NULLS+7+NR_CPUS)  /* number of gdt entries */


/* Local descriptor numbers */
#define LD_SC 0x0              /* System call gate for IBCS2 */

#endif /* !_XOK_MMU_H_ */
