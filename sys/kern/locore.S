
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


#include <xok/asm.h>
#include <xok/mmu.h>
#include <xok/trap.h>
#include <xok/env.h>
#include <xok/sys_syms.h>

/* The kernel is linked to be run up around KERNBASE + 1 Meg,
 * but when boot jumps to start, we are executing aroud 1 Meg.
 * RELOC(x) gives the address of symbol x before physical memory
 * is remapped at KERNBASE
 */
#define	RELOC(x) ((x) - KERNBASE)

	.data

/* A little stack to start things off... */
bootstack:
	.space	512
bootstacktop:

/* Physical address of the first process's page directory */
	.globl	_p0cr3_boot
	.comm	_p0cr3_boot,4
/* Virtual address of the first process's page directory */
	.globl	_p0pdir_boot
	.comm	_p0pdir_boot,4

/* Physical address of kernel stack map */
	.globl	_kstkmap
	.comm	_kstkmap,4

/* Address of the first free page of memory */
	.globl	_freemem
	.comm	_freemem,4

/* Model number returned by cpuid instruction */
	.globl	_cpuid_vers
	.comm	_cpuid_vers,4

	.globl	_cpuid_features
	.comm	_cpuid_features,4

/* See mmu.h for a complete description of these two.*/
	.globl	_vpt
	.set	_vpt, VPT
	.globl	_vpd
	.set	_vpd, (VPT + SRL(VPT, 10))

	.text

	.globl	_text_start
_text_start:
	.globl	start
start:
	movw	$0x1234,0x472			# warm boot

	/* The arguments passed in by boot are on the old stack.  For
	 * now just blow them away and switch to the new boot stack. */
	movl	$RELOC(bootstacktop),%esp

	/* Zero out the BSS segment */
	movl	$RELOC(_edata),%edi		# destination of string op

	# Replaced
	#   movl	$(((_end-_edata)+3)>>2),%ecx
	# with the three instructions below:	
	
	movl	$(3 + _end),%ecx		# repeat count for "rep"
	subl	$_edata,%ecx			#  = ceil ((_end-_edata)/4)
	shr	$2, %ecx

	xorl	%eax,%eax			# quickest way to get zero
	cld					# d=0 string op is ascending
	rep
	 stosl					# store it!

/* Are we on a pentium? */
	xorl	%eax,%eax
	movl	%eax,RELOC(_cpuid_vers)
	movl	%eax,RELOC(_cpuid_features)
	pushfl					# Put flags in %eax and %ecx
	popl	%eax
	movl	%eax,%ecx
	xorl	$FL_ID,%eax			# Can we twiddle the ID flag?
	pushl	%eax
	popfl
	pushfl
	popl	%eax				# %eax is twiddled flags reg
	pushl	%ecx				# Restore the flags in any case
	popfl
	cmpl	%eax,%ecx			# Did the twiddle work?
	movl	$0,%eax
	je	1f				# If not, <= 486
	cpuid					# else, get cpuid
	testl	%eax,%eax
	je	1f
	movl	$1,%eax
	cpuid
	movl	%eax,RELOC(_cpuid_vers)		# and save cpuid info
	movl	%edx,RELOC(_cpuid_features)
1:

/* We need to allocate a page directory, some page tables, and a
 * stack for the first process.  The first page of free memory is
 * at location:
 *
 *    TBLBASE = ((RELOC(_end) + PGMASK) & PGMASK)
 *
 * Unfortunately this is too complicated for a relocation, so we
 * will need to calculate TBLBASE when we need it.
 *
 * The memory after TBLBASE is used as follows:
 *
 *     TBLBASE + P0PDIR   - initial page directory (1 page).
 *     TBLBASE + KPSTKMAP - 1 page table for mapping the kernel
 *                          stack (only 2 PTE's actually used) (1 page).
 *     TBLBASE + KPSTK    - The stack used by the first process
 *                          (KSTKSIZE bytes).
 *     TBLBASE + KPSTKTOP - The top of the first process's stack.
 *     TBLBASE + PHYSMAP  - NPPT page tables mapping physical memory
 *                          (NPPT pages).
 */

#define P0PDIR 0
#define KPSTKMAP (P0PDIR + 1 * NBPG)
#define KPSTK (KPSTKMAP + 1 * NBPG)
#define KPSTKTOP (KPSTK + KSTKSIZE)
#define PHYSMAP (KPSTKTOP)
#define PHYSMAPEND (PHYSMAP + NPPT * NBPG)

	/* %esi = TBLBASE */
	movl	$(PGMASK + RELOC(_end)),%esi	# end of BSS...
	andl	$~PGMASK,%esi			# rounded up to next page

	/* Zero P0PDIR */
	leal	(P0PDIR)(%esi),%edi	# %edi = dest = TBLBASE + P0PDIR
	movl	%edi,RELOC(_p0cr3_boot)	# Save it for the C code
	movl	%edi,RELOC(_p0pdir_boot)# And save the kernel VA, too
	addl	$KERNBASE,RELOC(_p0pdir_boot)
	movl	$NLPG,%ecx		# %ecx = count = longwords / page
	xorl	%eax,%eax		# %eax = fill word = 0
	rep
	 stosl				# Zero out page directory

	/*
	 * Build page table directory at address
	 * %esi (=TBLBASE) + 0 (=P0PDIR).
	 *
	 * Start by inserting a PDE for KSTKMAP
	 */
	leal	P0PDIR+SRL(KSTACKTOP-NBPD, PDSHIFT-2)(%esi),%ebx
					# %ebx = address of KPSTK PDE
	leal	(KPSTKMAP|PG_P|PG_W|PG_U)(%esi),%eax
					# %eax = contents of KPSTK PDE
	movl	%eax,(%ebx)
					# store KPSTKMAP PDE
	/*
	 * Now we map (NPPT * NLPG) pages physical memory above KERNBASE.
	 * Use Page tables at PHYSMAP on a 486.  Use large translations
	 * on a Pentium and above.
	 */
	testl	$8,RELOC(_cpuid_features)
					# Do we have PSE?
	jnz	pentium_physmap		# If so, go use big translations

non_pentium_physmap:

	leal	P0PDIR+SRL(KERNBASE, PDSHIFT-2)(%esi),%ebx
					# %ebx = address of first physmap PDE
	leal	(PHYSMAP|PG_P|PG_W)(%esi),%eax
					# %eax = contents of first sysmap PDE
	movl	$NPPT,%ecx		# %ecx = # of PDEs for physmap
	/* Write'm out */
1:	movl	%eax,(%ebx)		# store PDE in P0PDIR
	addl	$NBPG,%eax		# Next PDE contains next page for PT
	addl	$4,%ebx
	loop	1b

	/*
	 * Now build NPPT page tables mapping the start of physical
	 * memory.
	 */
	movl	$(PG_P|PG_W),%eax	# %eax = First PTE (maps addr 0)
	leal	PHYSMAP(%esi),%ebx	# %ebx = Address of first PTE
	movl	$(NPPT*NLPG),%ecx	# %ecx = # of PTEs in NPPT pages
	/* Write'm out */
1:	movl	%eax,(%ebx)		# store PDE in P0PDIR
	addl	$NBPG, %eax		# Next PDE contains next page for PT
	addl	$4, %ebx
	loop	1b

	/* Skip the Pentium stuff */
	jmp	pentium_physmap_end

pentium_physmap:

	leal	P0PDIR+SRL(KERNBASE, PDSHIFT-2)(%esi),%ebx
					# %ebx = address of first physmap PDE
	movl	$(PG_P|PG_W|PG_PS),%eax	# Big mapping for PA 0-4Meg
	movl	$NPPT,%ecx		# # of high PDEs for phys mem
	/* Write'm out */
1:	movl	%eax,(%ebx)		# store PDE in P0PDIR
	addl	$NBPD,%eax		# Next PDE contains next 4 Megs
	addl	$4,%ebx
	loop	1b

	.byte	0xf,0x20,0xe0		# movl	%cr4,%eax
	orl	$CR4_PSE,%eax		# Enable paging extensions
	#andl	$~CR4_PSE,%eax		# Clear paging extensions?
	#andl	$~CR4_TSD,%eax		# Clear "Time Stamp Disable" bit
	.byte	0xf,0x22,0xe0		# movl	%eax,%cr4

pentium_physmap_end:

	/*
	 * Recursively insert PD in itself as a page table, to form
	 * a virtual page table at virtual address VPT
	 */
	leal	(P0PDIR|PG_P|PG_W)(%esi),%eax
					# %eax = PDE for VPT
	movl	%eax,P0PDIR+SRL(VPT, PDSHIFT-2)(%esi)
					# store PDE for VPT in page directory
	/* Now do the same for UVPT */
	leal	(P0PDIR|PG_P|PG_U)(%esi),%eax
	movl	%eax,P0PDIR+SRL(UVPT, PDSHIFT-2)(%esi)

	/*
	 * Zero out KPSTKMAP and write two PTEs for KPSTK
	 */
	leal	(KPSTKMAP)(%esi),%edi	# %edi = dest = TBLBASE + KPSTKMAP
	movl	%edi,RELOC(_kstkmap)	# Save kstackmap for C code
	movl	$(NLPG-(KSTKSIZE>>PGSHIFT)),%ecx
					# %ecx = count = longwords / page
					#		- pages / stack
	xorl	%eax,%eax		# %eax = fill word = 0
	rep
	 stosl				# Zero out page table

	leal	(KPSTK|PG_P|PG_W)(%esi),%eax
					# %eax = PTE for lower page of stack
	movl	$(KSTKSIZE>>PGSHIFT),%ecx
					# %ecx = count = pages / stack
1:	stosl				# Store PTE
	addl	$NBPG,%eax		# PTE for next physical page
	loop	1b

	/* Map the first 4 Megabytes at VA 0 (just for starting VM) */
	#leal	(PG_P|PG_W|PHYSMAP)(%esi),%eax	# XXX
	#movl	$(PG_P|PG_W|PG_PS),%eax		# XXX
	movl	P0PDIR+SRL(KERNBASE, PDSHIFT-2)(%esi),%eax
	movl	%eax,P0PDIR+0(%esi)

	/* Set the current page directory to P0PDIR */
	leal	P0PDIR(%esi),%eax	# %eax = page directory
	movl	%eax,%cr3		# pdbr (%cr3) = %eax

	/* Enable Paging by flipping bits in %cr0 */
	movl	%cr0,%eax
	orl	$(CR0_PE|CR0_PG|CR0_AM|CR0_WP|CR0_NE|CR0_TS|CR0_EM|CR0_MP),%eax
	andl	$(~(CR0_TS|CR0_EM)),%eax
	movl	%eax,%cr0		/* We've gone virtual! */

	/* Set up the flags register */
	pushfw
	popl	%eax
#	orl	$(FL_AC),%eax
	pushl	%eax
	popfw

	/* We're in virtual mode, but executing in equivalently mapped
	 * VM around 1 Meg.  We need to jump up to KERNBASE and set up
	 * a new stack.  We also need to set up the global descriptor
	 * table and load a sensible %cs segment descriptor.
	 */

	lgdt	PD_ADDR(_gdt_pd)	# Set up our global descriptor table
	movl	$(GD_UD|3),%eax		# fs/gs are only used by user code
	movl	%ax,%gs
	movl	%ax,%fs
	movl	$GD_KD,%eax		# Load kernel data segments with
	movl	%ax,%es			#   kernel data descriptor
	movl	%ax,%ds

	movl	$GD_KD,%eax		# Load kernel data segments with

	/* Leave a few words on the stack for a user utf trap frame */
	/* first 4 bytes on each stack is reserved for cpu context ptr */
	movl	$(KSTACKTOP-0x40),%esp # Stack actually mapped under kernel
	movl	%ax,%ss			# Load stack segment
	movl	$0x0,%ebp		# And nuke frame pointer

	/* The pseudo-physical address of the stacktop is also the first
	 * free page of physical memory.
	 */
	leal	(KERNBASE+PHYSMAPEND)(%esi),%eax
	testl	$8,RELOC(_cpuid_features)
	jz	1f
	leal	(KERNBASE+PHYSMAP)(%esi),%eax
1:
	movl	%eax,(_freemem)

	/* Now start executing code up at kernbase, with a flat %cs
         * register loaded from our own global descriptor table.
	 */
	pushl	$GD_KT
	pushl	$to_kernbase
	lret
to_kernbase:

	/* Now we are at KERNBASE, and so can unmap low memory. */
	movl	$0,(P0PDIR)(%esi)

	lidt	PD_ADDR(_idt_pd)	# Set up interrupt descriptor table

	/* Finally some C code!  Initialize our little kernel. */
	call	_i386_init

1:	jmp	1b



/* 
 * Test pure trap speed.
 */
ENTRY(fasttrap)
#if 0
	pushl	%ds
	pushl	%es
#if 0
	pushl 	$GD_KD
	popl	%ds
	pushl	$GD_KD
	popl	%es
#endif
	popl	%es
	popl	%ds
#endif
	iret


/*
 * System call entry point.  This routine was a bit slow in C.  This
 * should help things.
 *
 * On entry:
 *       %eax - # of system call
 *       %edx - 1st argument (if any)
 *       %ecx - 2nd argument (if any)
 *       %ebx - 3rd argument (if any)
 * (12(%esp)) - rest of arguments (if any) (user %esp is in trapframe)
 */

DBLC2:
 	.ascii "syscall path 0x%x\n"
.text
ENTRY(syscall)
#ifdef __HOST__
	cmpl $GD_NULLS*8, -4(%esp)
	jl _fault_trampoline
#endif
	pushl	%ds
	pushl	%es
	andl	$(MAX_SYSCALL-1),%eax	# Save one cycle with lazy decoding
	pushal				# Save all registers in case of fault
	pushl	$GD_KD			# Load up kernel segment registers
	popl	%ds
	pushl	$GD_KD
	popl	%es
	movl	_sctab+4(,%eax,8),%esi	# %esi is the number of args to syscall
	subl	$3,%esi			# Take the slow path for > 3 arguments
	ja	1f			#  (leaving # stackargs in %esi)
	pushl	%ebx			# Push third argument on stack
#if 0
#ifdef __SMP__
	lock
#endif
	incl	(_nsyscalls)
#endif	
	movl	_sctab(,%eax,8),%esi	# Address of function for syscall
	movl	$0, _syscall_pfcleanup	# Disable any old cleanup function
	call	%esi
	movl	8(%esp),%esi		# Restore %esi from pushal
	addl	$36,%esp
	popl	%es
	popl	%ds
	iret				
1:					
	movl	%esp,%ebp		# Remember stack pointer
	movl	52(%ebp),%edi		# User stack pointer


/* Allow page faults getting user args */
#if 1
	movl    $PFM_PROP,__page_fault_mode_H_ADDR
#else
	xorl	%eax,%eax
	str     %ax
	addl 	$-GD_TSS,%eax
	shrl 	$3,%eax
	movl 	___cpucxts(,%eax,4),%eax
	movl	$PFM_PROP, PFMODE_OFFSET(%eax)
#endif
	cmpl	$ULIM,%edi		# Make sure %edi is not in kern space
	ja	2f
1:
	movl	-4(%edi,%esi,4),%eax	# Take arguments on user stack
	pushl	%eax			# And push them on kernel stack
	decl	%esi			#  while (--%esi);
	jg	1b


/* Arguments over, no more faults */
#if 1
	movl    $PFM_NONE,__page_fault_mode_H_ADDR
#else
 	xorl	%eax,%eax
	str     %ax
	addl 	$-GD_TSS,%eax
	shrl 	$3,%eax
	movl 	___cpucxts(,%eax,4),%eax
	movl	$PFM_NONE, PFMODE_OFFSET(%eax)
#endif
	pushl	%ebx			# 3th argument still in %ebx
	movl	28(%ebp),%eax		# Restore %eax from pushal
	movl	_sctab(,%eax,8),%esi	# Address of fucntion for syscall
	movl	$0, _syscall_pfcleanup	# Disable any old cleanup function
	call	%esi

	movl	(%ebp),%edi		# Restore some registers
	movl	4(%ebp),%esi
	leal	32(%ebp),%esp		# Move %esp above pushal & seg regs
	movl	8(%ebp),%ebp
	popl	%ds
	popl	%es
	iret				
2:					# bogus user %esp and # args > 3
	movl	$ULIM,%edi		# Guarantee a fault
	jmp	1b
