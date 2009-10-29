#include <xok/asm.h>
#include <xok/mmu.h>
#include <xok/trap.h>
#include <xok/env.h>
#include <xok/sys_syms.h>

/*
* On entry, a trap frame is on the stack.
* If the trap is from user space then 
* the trap frame will contain the SS
* and ESP as well.  Note, neither breakpoint
* (trap 1) nor debug (trap 3) push an
* error code.
*
* stack growing down:	
* -------------------
* (tt) ss	  # optional: user mode only
* (tt) esp     # optional: user mode only	
* (tt) eflags
* (tt) cs
* (tt) eip     # <-------%esp
*
* We trapped from kernel mode if the cs (on the stack)
* contains GD_KT.
*
* If the trap was from kernel mode, we just need to jump to the
* appropriate (kernel) handler.  If the trap was from user mode, we need to 
* copy the trap frame onto the user stack and upcall to the 
* appropriate (user space) handler.  This way it is indistinguishable
* to the user that the trap was upcalled and not directly vectored to
* user space.
*/
	
#define EH(x) .globl x; x:
#define E(x) EH(____aaa ## x)
	
# This code assumes that if we are executing the kernel text segment,
# then %es and %ds will both contian GD_KD.  IS THIS TRUE? 
# well... i think it is MOST of the time, but not always.
# For example, when doing an upcall (env_pop_tf) it is not true.
# The result is that you cant single step through the kernel
# in these places.  The assumption is used by je __catchException
# which uses the %ds, i believe)


.text
ENTRY(debug_trap)
	cmpl	$GD_KT,4(%esp)	# does tt_cs == GD_KT ?
	je __catchException1	# debug trap from kernel mode 
	/* trap was from user mode */
	
	andl	$~FL_TF,8(%esp)			# clear the trace flag
	pushl	%edx
	movl	$UIDT_VECT_TO_ADDR(0x01),%edx	# get the addr of the user handler 
	jmp copy_trap_frame_to_user_stack;	
	
	
ENTRY(breakpoint_trap)	
	cmpl	$GD_KT,4(%esp)	# does tt_cs == GD_KT ?
	je __catchException3	# breakpoint trap from kernel mode 
	/* trap was from user mode */
	pushl	%edx		
	movl	$UIDT_VECT_TO_ADDR(0x03),%edx	# get the addr of the user handler 
	jmp copy_trap_frame_to_user_stack;	
		

/* stack (growing down)	
* (tt) ss	  
* (tt) esp     
* (tt) eflags
* (tt) cs
* (tt) eip     
* saved edx	# <-------%esp
*/
	
/*	
* %edx contains the address of the user handler (ie. the destination
* of the upcall)
*/	
	
	
copy_trap_frame_to_user_stack:			
	pushl	%ds
	pushl	%es
	pushl	$GD_KD		# load the kernel segment descriptor
	popl	%ds
	pushl	$GD_KD		# load the kernel segment descriptor
	popl	%es
	
	pushl	%eax		# will hold user stack pointer 
	pushl	%ebx		# used as a transfer register 

#if 1
    	pushl	__page_fault_mode_H_ADDR
    	movl	$PFM_PROP,__page_fault_mode_H_ADDR
#else
	xorl	%eax,%eax
	str     %ax
	addl 	$-GD_TSS,%eax
	shrl 	$3,%eax
	movl 	___cpucxts(,%eax,4),%eax
	pushl	PFMODE_OFFSET(%eax)
	movl	$PFM_PROP, PFMODE_OFFSET(%eax)
#endif
        
	
	subl	$12,36(%esp)	# (tt) esp-=12  (ie. allocate space for trap frame)
	


	movl	36(%esp),%eax	# %eax <-- (tt) (esp-12), now we have a pointer
				# to the users stack
	
# hmmm.... do we need to be more careful with this (given the minus 12)?
	cmpl	$ULIM,%eax	# Make sure %eax is not in kern space
	ja	2f
1:	
	
	movl	32(%esp),%ebx	# move (tt) eflags onto user stack
	movl	%ebx, 8(%eax)
	
	movl	28(%esp),%ebx	# move (tt) cs onto user stack
	movl	%ebx, 4(%eax)
	
	movl	24(%esp),%ebx	# move (tt) eip onto user stack
	movl	%ebx, 0(%eax)
	
        movl	%edx,24(%esp)	# overwrite the (tt) eip with the addr we iret to 

#if 1
	popl		__page_fault_mode_H_ADDR
#else
	xorl	%eax,%eax
	str     %ax
	addl 	$-GD_TSS,%eax
	shrl 	$3,%eax
	movl 	___cpucxts(,%eax,4),%eax
	popl	PFMODE_OFFSET(%eax)
#endif

	popl	%ebx
	popl	%eax
	
	popl	%es
	popl	%ds
	popl	%edx
	iret
	
2:	
	movl	$ULIM,%eax		# Guarantee a fault
	jmp	1b
