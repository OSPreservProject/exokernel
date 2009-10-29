
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


#include <sys/asm.h>
#include <sys/mmu.h>
#include <sys/syscall.h>
#include <sys/syscallno.h>	
#include <sys/fpu.h>	

	.data
_osid: .asciz "u_in_critical got set\n"	
	.text
	
/*
 * By default the kernel starts running things right at the start
 * of the text segment.  If we are entered here, call start.
 */
	jmp	start

ENTRY(xue_epilogue)
	pushl	___u_ppc
	pushfl
	cmpl	$0, (___u_in_critical)
	je	_xue_yield
	movl	$1, (___u_interrupted)
	jmp	_xue_go_back
	.globl _xue_yield
_xue_yield:	
	pushal
	cmpl	$0, (___u_fpu)
	je	_xue_no_fpu
	fwait
	subl	$FPU_SAVE_SZ, %esp
	fsave	(%esp)
_xue_no_fpu:	
	int	$T_YIELD
	ALIGN_TEXT
	.globl _xue_prologue
_xue_prologue:
#if 0	
	movl	(___si_system_ticks+4), %eax
	movl	(___si_system_ticks), %edx
	movl	(___u_next_timeout+4), %ebx
	movl	(___u_next_timeout), %ecx
	cmpl	%ecx, %edx
	jl	_xue_timeout
	jg	_xue_notimeout
	cmpl	%ebx, %eax
	jg	_xue_notimeout
_xue_timeout:
	call	_DoTimeout
_xue_notimeout:
#endif
	call	___CheckAlarm
	cmpl	$0, (___u_pending_sigs)
	je	_xue_no_sigs
	call	_DeliverPendingSignal
_xue_no_sigs:
	cmpl	$0, (___u_fpu)
	je	_xue_no_fpu2	
	frstor	(%esp)
	addl	$FPU_SAVE_SZ, %esp
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
	movl	%eax,32(%esp)		# Save caller-saved registers
	movl	%edx,28(%esp)
	movl	%ecx,24(%esp)

	movl	16(%esp),%edx		# take trap %esp
	subl	$12,%edx		# make room for return address and regs
	movl	%edx,16(%esp)		# and save it for later

	call	_page_fault_handler	# Arguments already set up

	movl	16(%esp),%edx
	movl	32(%esp),%eax		# stick %eax on traptime stack
	movl	%eax,4(%edx)
	movl	28(%esp),%eax		# stick %edx on traptime stack
	movl	%eax,(%edx)
	movl	12(%esp),%eax		# stick ret address on traptime stack
	movl	%eax,8(%edx)

	movl	24(%esp),%ecx		# Restore caller-saved register
	addl	$8,%esp			# Need it for popfl
	popfl				# Restore flags
	movl	4(%esp),%edx		# Get trap %esp
	movl	8(%esp),%eax		# We are done with the exception stack
	movl	%eax,___u_xesp

	movl	%edx,%esp		# Switch back to traptime stack
	popl	%edx			# Restore registers ...
	popl	%eax
	ret				# ... and go!

