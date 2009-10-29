
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
#include <xok/syscall.h>
#define IPC_OUTSTANDING_DONE_RET  0x1 /* The return ipc2 has occurred */

	.data

	.comm	_ipc1_in,4
	.comm	_ipc2_in,4

	.text

/*
 * IPC1 entry point.  On entry:
 *
 *      %edi - Calling environment ID
 *      %esi - Opaque call ID, must be returned as-is
 *      %eax - Argument 1
 *      %edx - Argument 2
 *      %ecx - Argument 3
 *      %ebx - Number of extra args (in UAREA)
 */
ENTRY(ipc1_entry)
	pushl	%edi
	pushl	%ebx
	call	*(_ipc1_in)
	addl	$8, %esp
	movl	%edx, %ebx
	cmpl	$0, (__exos_fpu_used_ctxt)
	je	ipc2_no_fpu
	movl	$0, (__exos_fpu_used_ctxt)
	fnsave	__exos_fpus # save fpu state without causing exception
	fwait		# (conservative) wait for write to memory to complete
ipc2_no_fpu:
	int	$T_IPC2S
1:	int	$T_YIELD
	jmp	1b

/*
 * Asynchronous IPC out.  On entry:
 *
 *       %eax - Argument 1
 *       %edx - Argument 2
 *       %ecx - Argument 3
 *    4(%esp) - Number of extra args
 *    8(%esp) - Target environment ID
 *   12(%esp) - Table entry
 *   16(%esp) - Location of extra args
 */
ENTRY(__asm_ipcout)
	pushl	%ebx			# Save the callee-saved registers
	pushl	%ebp			#   (as they will get trashed).
	pushl	%esi
	pushl	%edi

	movl	20(%esp),%ebx		# %ebx = number of extra args
	movl	24(%esp),%edi		# %edi = destination_env
	movl	28(%esp),%esi		# %esi = table entry
	pushl	$1			# tell prologue we ipced
	pushl	36(%esp)		# Location of extra args
	cmpl	$0, (__exos_fpu_used_ctxt)
	je	ipc1_no_fpu
	movl	$0, (__exos_fpu_used_ctxt)
	fnsave	__exos_fpus # save fpu state without causing exception
	fwait		# (conservative) wait for write to memory to complete
ipc1_no_fpu:
	int	$T_IPC1			# Make IPC
	.globl ipc1_wakeup
ipc1_return:
	popl	%edi			# skip location of extra args
	popl	%edi			# skip msg to prologue
	popl	%edi			# restore registers
	popl	%esi
	popl	%ebp
	popl	%ebx
	ret
ipc1_wakeup:
	xorl	%eax, %eax		# ipc successful
	popl	%edi			# restore registers
	popl	%esi
	popl	%ebp
	popl	%ebx
	ret

/*
 * IPC2 entry point.  (return from asynchronous IPC1) On entry:
 *
 *      %edi - Calling environment ID
 *      %esi - Opaque call ID, must be returned as-is
 *      %eax - Return value (Argument 1)
 *      %edx - Other possible return value (long long) (Argument 2)
 *      %ecx - Argument 3
 *      %ebx - Number of extra args (or Receiver Success for ipc2)
 */
	ALIGN_TEXT
ENTRY(ipc2_entry)
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	call	*(_ipc2_in)
	popl	%ebx
	addl	$8,%esp
	/* three possibilities
	  1 - ignore bogus ipc2 call
	  2 - we've woken up since original ipc1 call already
	  3 - ipc2 return was quick
	*/
	cmpl	$IPC_OUTSTANDING_DONE_RET, %eax
	jne	2f			# cases 1 & 2
	movl	%ebx, %eax		# case 3
	testl   %eax, %eax		# callee return value must be > 0
	jns     1f
        negl    %eax
1:	jmp	ipc1_return
2:	int	$T_YIELD

/*
 * Synchronous IPC out to an ASH.  On entry:
 *
 *      %eax - Argument 1
 *      %edx - Argument 2
 *      %ecx - Argument 3
 *   4(%esp) - Argument 4
 *   8(%esp) - Target environment ID
 */
ENTRY(__asm_ashipc)
	pushl	%ebx			# Save the callee-saved registers
	pushl	%ebp			#   (as they will get trashed).
	pushl	%esi
	pushl	%edi

	movl	20(%esp),%ebx		# %ebx = arg4;
	movl	24(%esp),%edi		# %edi = destination_env;
	int	$T_IPCAS		# Make ASH IPC
	popl	%edi			# Restore callee-saved registers
	popl	%esi
	popl	%ebp
	popl	%ebx
	ret				# Return value already in %eax
