
/*
 * Copyright MIT 1999
 */

#include <sys/asm.h>
#include <xok/mmu.h>
#include <xok/syscall.h>
#include <xok/syscallno.h>	
#include <vos/fpu.h>
#include <vos/ipc.h>

	.text
	.globl	_xue_epilogue_u_ppc
#if 0
	.globl	_xue_epilogue_u_donate
	.globl	_xue_epilogue_u_in_critical
	.globl	_xue_epilogue_u_interrupted
#endif

/* Save and restore our registers. This is normally run in response to the
 * kernel doing an up call saying that our quantum has expired, but yield also
 * jumps here.  We do not give up the processor immediately in response to an
 * upcall if we're in a critical section (__u_in_critical is set). Instead, we
 * delay releasing the processor until the critical section is exited.  See
 * exos/critical.h for more details and interfaces. */
	
ENTRY(xue_epilogue)
#if 1
	pushl	___u_ppc
#else

	.byte	0xff, 0x35	# pushl
_xue_epilogue_u_ppc:
	.long 	0x0
#endif
	pushfl
	movl	$-1, (___u_donate)	
	cmpl	$0, (___u_in_critical)
	je	_xue_yield
	movl	$1, (___u_interrupted)
	jmp	_xue_go_back
	
	.globl _xue_yield
_xue_yield:
	pushal

	cmpl	$0, (_vos_fpu_used_ctxt)
	je	_xue_yield_no_fpu
	movl	$0, (_vos_fpu_used_ctxt)
	fnsave	_vos_fpus # save fpu state without causing exception
	fwait	          # (conservative) wait for wrt to memory to complete
_xue_yield_no_fpu:	
	pushl	$YIELD_WAKEUP  	# this is how we distinguish yield from IPCs
	pushl	$0  		# ditto
	int	$T_YIELD
	ALIGN_TEXT
	
	.globl _xue_prologue
_xue_prologue:
	pushl	$0
        call	_ipc_msgring_callback
	popl	%eax
        
	/************************************
	 * we distinguish returns from yield 
	 * and return from IPC.
	 ************************************/
	popl	%eax
	popl	%eax          /* if YIELD_WAKEUP, return from yield */
	cmpl	$IPC_WAKEUP, %eax
	je	_ipc1_wakeup  /* return from IPC */
	.globl  _xue_yield_wakeup
_xue_yield_wakeup:
	popal
_xue_go_back:		
	popfl
	ret


ENTRY(xue_fyield)
	pushal
	cmpl	$0, (_vos_fpu_used_ctxt)
	je	_xue_fyield_no_fpu
	movl	$0, (_vos_fpu_used_ctxt)
	fnsave	_vos_fpus # save fpu state without causing exception
	fwait	          # (conservative) wait for wrt to memory to complete
_xue_fyield_no_fpu:	
	pushl   $YIELD_WAKEUP
	pushl   $0
	int     $T_FASTYIELD
        jmp	_xue_prologue

/* 
 * Entry point for user page fault handler.  On entry, the stack
 * looks like this (high addresses on top).
 *
 *    unused (for trap %eip if no separate exception stack)
 *    unused (for trap %eax)
 *    unused (for trap %edx)
 *    unused (for trap %ecx)
 *    pevious xsp
 *    trap %esp
 *    trap %eip
 *    trap eflags
 *    errorcode
 *    %cr2        <---- %esp
 */
ENTRY(xue_fault)
        movl    %eax,32(%esp)           # Save caller-saved registers
        movl    %edx,28(%esp)
        movl    %ecx,24(%esp)
 
        movl    16(%esp),%edx           # take trap %esp
        subl    $12,%edx                # make room for return address and regs
        movl    %edx,16(%esp)           # and save it for later
 
        call    _page_fault_handler     # Arguments already set up
 
        movl    16(%esp),%edx
        movl    32(%esp),%eax           # stick %eax on traptime stack
        movl    %eax,4(%edx)
        movl    28(%esp),%eax           # stick %edx on traptime stack
        movl    %eax,(%edx)
        movl    12(%esp),%eax           # stick ret address on traptime stack
        movl    %eax,8(%edx)
 
        movl    24(%esp),%ecx           # Restore caller-saved register
        addl    $8,%esp                 # Need it for popfl
        popfl                           # Restore flags
        movl    4(%esp),%edx            # Get trap %esp
        movl    8(%esp),%eax            # We are done with the exception stack
        movl    %eax,___u_xesp
 
        movl    %edx,%esp               # Switch back to traptime stack
        popl    %edx                    # Restore registers ...
        popl    %eax
        ret                             # ... and go!
ENTRY(xue_fault_end)
 


