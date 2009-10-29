
#include <sys/asm.h>
#include <xok/mmu.h>
#include <xok/syscall.h>
#include <xok/syscallno.h>	

	.text

/* Save and restore our registers (fpu registers only if u.u_fpu is set).
   This is normally run in response to the kernel doing an up call saying
   that our quantum has expired, but yield also jumps here.
   We do not give up the processor immediately in response to an upcall
   if we're in a critical section (__u_in_critical is set). Instead, we
   delay releasing the processor until the critical section is exited.
   See exos/critical.h for more details and interfaces. */
	
ENTRY(xue_epilogue)
	pushl	___u_ppc
	pushfl
	movl	$-1, (___u_donate)	
	cmpl	$0, (___u_in_critical)
	je	_xue_yield
	movl	$1, (___u_interrupted)
	jmp	_xue_go_back
	.globl _xue_yield
_xue_yield:	
	pushal
#if 0	
	cmpl	$0, (___u_fpu)
	je	_xue_no_fpu
	fwait
	subl	$FPU_SAVE_SZ, %esp
	fsave	(%esp)
#endif	
_xue_no_fpu:	
	int	$T_YIELD
	ALIGN_TEXT
	.globl _xue_prologue
_xue_prologue:
#if 0	
	cmpl	$0, (___u_fpu)
	je	_xue_no_fpu2
	frstor	(%esp)
	addl	$FPU_SAVE_SZ, %esp
#endif	
_xue_no_fpu2:		
	popal
_xue_go_back:		
	popfl
	ret

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
 


