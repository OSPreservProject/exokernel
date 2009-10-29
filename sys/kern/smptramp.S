#include <xok/asm.h>
#include <xok/mmu.h>
#include <xok/trap.h>
#include <xok/env.h>

#ifdef __SMP__

#define addr32 .byte 0x67
#define data32 .byte 0x66

/* virtual to physical translation */
#define	RELOC(x) ((x) - KERNBASE)

/* for debugging use */
#define CMOS_REG	(0x70)
#define CMOS_DATA	(0x71)

#define CHECKPOINT(A,D)		\
	movb	$(A),%al ;	\
	outb	%al,$CMOS_REG ;	\
	movb	$(D),%al ;	\
	outb	%al,$CMOS_DATA



/***************************************
 * Below is the code that resides in   *
 * kernel space @ +1 Meg. The AP boot  *
 * strap code calls this to finish the *
 * initialisation process              *
 ***************************************/


        .align  4
        .text
	.globl  kern_space_32bit_tramp_startup

kern_space_32bit_tramp_startup:

 	CHECKPOINT(0x37,0xA4)

	/* for debugging only - we tell BSP our state */
        xorl %eax,%eax
	movl $0xC3C3C3C3,%eax		
	movl %eax,0x9FFFC


        /*****************************
         * in protected mode, 32 bit *
	 *****************************/

	xorl %eax,%eax
	.byte 0xf,0x20,0xe0		# movl	%cr4,%eax
	orl  $CR4_PSE,%eax		# Enable paging extensions
	.byte 0xf,0x22,0xe0		# movl	%eax,%cr4

	/* set current page directory, then turn to virtual */
        xorl %eax,%eax
	movl RELOC(_p0cr3_smp),%eax		# load current page directory
	movl %eax,%cr3

        xorl %eax,%eax
	movl %cr0,%eax
	orl  $(CR0_PE|CR0_PG|CR0_AM|CR0_WP|CR0_NE|CR0_TS|CR0_EM|CR0_MP),%eax
	andl $(~(CR0_TS|CR0_EM)),%eax
	movl %eax,%cr0			# we are virtual
 	CHECKPOINT(0x38,0xA5)


        /* just like transfer to protected mode, we need 
	 * to flush instruction pipeline, therefore we 
	 * make an intrasegment jump
	 */

	pushl	$virtual_mode
	ret

virtual_mode:

	xorl %eax,%eax      
	movl $0xD4D4D4D4,%eax		# signal to BSP we received the IPI
	movl %eax,0xF809FFFC


        /*******************
	 * in virtual mode *
         *******************/

	/* load the real GDT, and again reset segment registers */
	lgdt PD_ADDR(_gdt_pd)
	lidt PD_ADDR(_idt_pd)
	jmp  1f
	nop

1:
	xorl %eax,%eax
	movl $(GD_UD|3),%eax		# fs/gs are only used by user code
	movl %ax,%gs
	movl %ax,%fs
	xorl %eax,%eax
	movl $GD_KD,%eax		# load kernel data segments
	movl %ax,%es
	movl %ax,%ds
	movl %ax,%ss


 	CHECKPOINT(0x39,0xA6)

	/* now, we want to go back to our own little stack */
	/* ecx still has the physical address of the stack assigned */

	addl $KERNBASE,%ecx
	movl %ecx,%esp			# set up stack pointer
	movl $0x0,%ebp			# nuke frame pointer

	xorl %eax,%eax      
	movl $0xE5E5E5E5,%eax		# signal to BSP we received the IPI
	movl %eax,0xF809FFFC

	xorl %eax,%eax
	lldt %ax

	pushl %eax			# empty params to C
	pushl %eax
	pushl %eax
	cld				# direction flag cleared 
	call _i386_init_secondary

	/* previous call should never return, but if it does, loop */
2:	jmp 2b






/**************************************
 * Below is the trampoline bootstrap  *
 * code that is copied into each AP's *
 * own page during runtime            *
 **************************************/

/* This kernel is copied (in smp.c) to right below 640K, at
 * 4K page boundary. When AP jumps to start at the begining
 * of this code, CS:EIP is set to the beginning of that page
 * (we hope, or else we are doomed). We are also in real 
 * mode with no stack to start with, and 16 bit
 */


        .align  4
	.text

	.globl	_smp_trampoline_start
	.globl	smp_trampoline_start
        .globl  _smp_tramp_lgdt_arg

_smp_trampoline_start:
smp_trampoline_start:
 	CHECKPOINT(0x34,0xA1)

        /*********************
	 * real mode, 16 bit *
         *********************/

        cli

	/* because we are in 16 bit mode we need to addr32 and data32 */

	/* first, let's save the pa of the page we are in */
	xor %ax,%ax
	xor %cx,%cx
	mov %cs,%ax 

        /* set up our segment registers */
        mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	mov %ax,%gs
	mov %ax,%ss
	mov %ax,%cx

        /* save the linear addr of this page */
	data32
	addl %ecx,%ecx
	data32
	addl %ecx,%ecx
	data32
	addl %ecx,%ecx
	data32
	addl %ecx,%ecx

        /* set up a stack */
	mov $(_smp_tramp_boot_stack-smp_trampoline_start),%sp

	/* load descriptor table. 
	 * for now, just load whatever we've got 
	 * we will load the real one later when we are
	 * in protected mode and have access to the real tables 
	 */

        addr32
	data32

        /* XXX - self assembled below (copied from freebsd) */
	/* lgdt tramp_gdt_48-smp_trampoline_start */

        .byte 0x0f, 0x01, 0x15
_smp_tramp_lgdt_arg:		# changed later in smp.c
	.long 0x12345678

        
	/* for debugging only - we tell BSP our state */
	data32
	xorl %eax,%eax      
	data32
	movl $0xA1A1A1A1,%eax
	data32
	movl %eax,0xFFC


	/* switch to protected mode */

        data32
	movl %cr0,%eax
	data32
	orl  $CR0_PE,%eax
        data32
	movl %eax,%cr0


 	CHECKPOINT(0x35,0xA2)

        /* now we make an intrasegment jump to flush 
	 * processor pipeline and reload the CS register 
	 */
	data32
	pushl	$0x18		# trampoline code selector
	data32
	pushl	$(flush_instr-smp_trampoline_start)
	data32
	lret


flush_instr: 
 	CHECKPOINT(0x36,0xA3)


        /*********************************************
	 * protected mode, 32 bit, but still running *
	 * with bad segment descriptor               *
	 *********************************************/

	xorl %eax,%eax
	mov  $GD_KD,%eax		# load kernel data segments
	movw %ax,%ds
	movw %ax,%es
	movw %ax,%fs
	movw %ax,%gs
	movw %ax,%ss

        /* %ecx still has physical address for page */
	addl $(NBPG-4),%ecx		# goto end of page
	movl %ecx,%esp			# set up stack pointer
	movl $0x0,%ebp			# nuke frame pointer

	/* for debugging only - we tell BSP our state */
        xorl %eax,%eax
	movl $0xB2B2B2B2,%eax		
	movl %eax,0x9FFFC



        /**********************************
	 * jump to kernel space at +1 Meg *
	 **********************************/

	pushl	$GD_KT		# kernel text selector
	pushl	$RELOC(kern_space_32bit_tramp_startup)
	lret

        hlt      			# never suppose to be here



/***************************************************************
 *  Below we store a temporary table of global descriptors    
 *  We use this to load into kernel space. In kernel space    
 *  we can reload the real GDT and IDT.                       
 *
 *
 *  Segment Descriptor
 *
 * 31          24         19   16                 7           0
 * ------------------------------------------------------------
 * |             | |B| |A|       | |   |1|0|E|W|A|            |
 * | BASE 31..24 |G|/|0|V| LIMIT |P|DPL|  TYPE   | BASE 23:16 |
 * |             | |D| |L| 19..16| |   |1|1|C|R|A|            |
 * ------------------------------------------------------------
 * |                             |                            |
 * |        BASE 15..0           |       LIMIT 15..0          |
 * |                             |                            |
 * ------------------------------------------------------------
 *
 **************************************************************/

        .globl _smp_tramp_gdt_48
        .globl _smp_tramp_gdt_48_base_addr
        .globl _smp_tramp_gdt_base_addr
	.globl _smp_tramp_boot_data_seg
	.globl _smp_tramp_boot_code


        .align  4
_smp_tramp_gdt_base_addr:
				# 0x0 null descriptor
        .word   0x0
	.word   0x0
	.byte   0x0
	.byte   0x0
	.byte   0x0
	.byte   0x0
				# 0x8  kernel code segment
	.word	0xffff		# segment limit 0..15 
	.word	0x0000		# segment base 0..15 
	.byte	0x0		# segment base 16..23; set for 0K 
	.byte	0x9f		# flags; Type	
	.byte	0xcf		# flags; Limit	
	.byte	0x0		# segment base 24..32 

				# 0x10 kernel data segment
	.word	0xffff		# segment limit 0..15 
	.word	0x0000		# segment base 0..15 
	.byte	0x0		# segment base 16..23; set for 0k 
	.byte	0x93		# flags; Type  
	.byte	0xcf		# flags; Limit 
	.byte	0x0		# segment base 24..32 

				# 0x18 trampoline code 
	.word	0xffff		# segment limit 0..15 
_smp_tramp_boot_code:		# modified later
	.word	0xBBBB		# segment base 0..15 
	.byte	0xAA		# segment base 16...23; set for 0x000xx000 
	.byte	0x9e		# flags; Type 
	.byte	0xcf		# flags; Limit
	.byte	0x0		# segment base 24..32 

				# 0x20 trampoline data
	.word	0xffff	
_smp_tramp_boot_data_seg:	# modified later
	.word	0xBBBB		# segment base 0..15 
	.byte	0xAA		# segment base 16...23; set for 0x000xx000 
	.byte	0x93	
	.byte	0xcf	
	.byte	0x0


_smp_tramp_gdt_48:
	.word	0x0028			
_smp_tramp_gdt_48_base_addr:		# changed later in smp.c
	.long	0xA5B4B5A4		# gdt base = tramp_gdt


/***********************************
 * finally, a little stack to use  *
 ***********************************/

	.space 0x100
_smp_tramp_boot_stack:



	.globl _smp_trampoline_end
_smp_trampoline_end:

#endif 
