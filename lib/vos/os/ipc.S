
/*
 * Copyright MIT 1999
 */

#include <xok/asm.h>
#include <xok/mmu.h>
#include <xok/syscall.h>
#include <xok/kerrno.h>
#include <vos/errno.h>
#include <vos/ipc.h>


/*
 * Assembly implementation of IPC
 *
 * Should update ipc.txt if modifications are made
 */

	.data
	.comm	_ipc1_in,4
	.comm	_ipc2_in,4
	.text
/*
 * Receiver side: IPC1 entry point. On entry:
 *
 *      %edi - Calling environment ID
 *      %eax - Argument 0 
 *      %edx - Argument 1
 *      %ecx - Argument 2
 *      %ebx - Number of extra args (not passed into ipc1_in)
 *      %esi - Request ID (not passed into ipc1_in)
 */
ENTRY(ipc1_entry)
	pushl	%edi       /* last arg passed via stack, rest via registers */
	                   /* ignore ebx and esi */
	call	*(_ipc1_in)
	addl	$4, %esp

	cmpl	$0, (_vos_fpu_used_ctxt) 
	je	ipc2_no_fpu
	movl	$0, (_vos_fpu_used_ctxt)
	fnsave	_vos_fpus # save fpu state without causing exception
	fwait	          # (conservative) wait for wrt to memory to complete
ipc2_no_fpu:

	int	$T_IPC2S   /* return via IPC2 */
1:	int	$T_YIELD
	jmp	1b




/*
 * Receiver side: IPC2 entry point. On entry:
 *
 *      %edi - Calling environment ID
 *      %eax - Argument 0
 *      %edx - Argument 1
 *      %ecx - Argument 2
 *      %ebx - Argument 3 
 *      %esi - Argument 4 - caller request ID, passed onto the handler 
 *
 * On entry to ipc2_in:
 *
 *      %eax - Argument 0
 *	%edx - Argument 1
 *      %ecx - Request ID
 *   4(%esp) - Caller envid
 */
	
	ALIGN_TEXT
ENTRY(ipc2_entry)
	movl    %esi,%ecx
	pushl	%edi
	call	*(_ipc2_in) 
	popl	%edi
	jmp	ipc1_return



/*
 * Sending side: asynchronous IPC out. On entry:
 *
 *       %eax - Argument 0
 *       %edx - Argument 1
 *       %ecx - Argument 2
 *    4(%esp) - Argument 3, or Number of extra args
 *    8(%esp) - Argument 4, or Caller request ID
 *   12(%esp) - Target environment ID
 *   16(%esp) - Location of extra args
 */
ENTRY(__asm_ipc_local)
	pushl	%ebx			# save the callee-saved registers
	pushl	%ebp
	pushl	%esi
	pushl	%edi

	movl	20(%esp),%ebx		# %ebx = number of extra args
	movl	24(%esp),%esi		# %esi = caller request ID
	movl	28(%esp),%edi		# %edi = destination_env

        /************************************
	 * we distinguish us from a regular
	 * yield by pushing the following 
	 * values... in real xue_yield 2 0's
	 * are pushed (see entry.S)
	 ************************************/

	pushl	$IPC_WAKEUP		# tell prologue we did an IPC
	pushl	36(%esp)		# Location of extra args

	cmpl	$0, (_vos_fpu_used_ctxt)
	je	ipc1_no_fpu
	movl	$0, (_vos_fpu_used_ctxt)
	fnsave	_vos_fpus # save fpu state without causing exception
	fwait		  # (conservative) wait for wrt to memory to complete
ipc1_no_fpu:

	int	$T_IPC1			# Make IPC
ipc1_return:
	popl	%edi			# skip location of extra args
	popl	%edi			# flag to prologue
        cmpl    $YIELD_WAKEUP, %edi     # already woken up before
	je	_xue_yield_wakeup
	popl	%edi			# restore registers
	popl	%esi
	popl	%ebp
	popl	%ebx
	ret

	.globl _ipc1_wakeup
_ipc1_wakeup:
	movl	$V_WOULDBLOCK, (_errno)
	movl	$-1, %eax		# ipc woken up by prologue
	popl	%edi			# restore registers
	popl	%esi
	popl	%ebp
	popl	%ebx
	ret


