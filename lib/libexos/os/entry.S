
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
#include <xok/syscallno.h>	
#include <exos/fpu.h>
#include <exos/callcountdefs.h>
#include <exos/ptrace.h>

	.data
_osid: .asciz "u_in_critical got set\n"
	.text

ENTRY(xue_epilogue)
	pushl	___u_ppc
	pushfl
#ifdef EPILOGUE_COUNT
	incl	___u_epilogue_count	
#endif
	movl	$-1, (___u_donate)	
	cmpl	$0, (___u_in_critical)
	je	_xue_yield
	movl	$1, (___u_interrupted)
#ifdef EPILOGUE_ABORT_COUNT
	incl	___u_epilogue_abort_count	
#endif
	jmp	_xue_go_back

	.globl _xue_yield
_xue_yield:	
#ifdef YIELD_COUNT
	incl	___u_yield_count	
#endif
	pushal
	cmpl	$0, (__exos_fpu_used_ctxt)
	je	_xue_no_fpu
	movl	$0, (__exos_fpu_used_ctxt)
	fnsave	__exos_fpus # save fpu state without causing exception
	fwait		# (conservative) wait for write to memory to complete
_xue_no_fpu:
	pushl	$0	# needed to differentiate from ipc call
	pushl	$0	# needed to differentiate from ipc call
	int	$T_YIELD

	ALIGN_TEXT
	.globl _xue_prologue
_xue_prologue:
	call	___CheckITimers
	call	__exos_check_paging
	cmpl	$0, (_pending_nonblocked_signal)
	je	_xue_no_sigs
	call	_DeliverPendingSignal
_xue_no_sigs:
	cmp	$0, (___u_revoked_pages)
	je	_no_revocation
	call	__do_revocation
_no_revocation:
	popl	%eax
	popl	%eax
	cmpl	$0, %eax
	jne	ipc1_wakeup
	popal

	testl	$EXOS_PT_BEING_PTRACED, ___u_ptrace_flags
	je	_xue_go_back
	testl	$EXOS_PT_PCONTINUE, ___u_ptrace_flags
	jne	_xue_go_back
	# set upstack as bp handler expects and jump to it
	movl	%eax, __exos_ptrace_eax_save
	movl	%ebx, __exos_ptrace_ebx_save
	popl	%eax
	popl	%ebx
	pushl	%eax
	pushl	%cs
	pushl	%ebx
	movl	__exos_ptrace_eax_save, %eax
	movl	__exos_ptrace_ebx_save, %ebx
	jmpl	_ptrace_breakpoint_handler_s
_xue_go_back:		
	popfl
	ret

 /* 
 * Entry point for user page fault handler.  On entry, the stack
 * looks like this (high addresses on top).
 *
 *    unused (for trap %eflags if no separate exception stack)
 *    unused (for trap %cs if no separate exception stack)
 *    unused (for trap %eip if no separate exception stack)
 *    unused (for trap %eax)
 *    unused (for trap %edx)
 *    unused (for trap %ecx)
 *    previous xsp
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

	call	_page_fault_handler	# Arguments already set up

	cmpl	$0,%eax                 # check if we should send segv to debugger
	je	1f
	
	movl	16(%esp),%edx		# take trap %esp
	subl	$20,%edx		# make room for return address and regs
	
	movl	8(%esp),%eax		# stick eflags on traptime stack
	movl	%eax,16(%edx)
	movl	%cs,12(%edx)		# stick cs on traptime stack
	movl	12(%esp),%eax		# stick ret address on traptime stack
	movl	%eax,8(%edx)
	movl	32(%esp),%eax		# stick %eax on traptime stack
	movl	%eax,4(%edx)
	movl	28(%esp),%eax		# stick %edx on traptime stack
	movl	%eax,(%edx)

	movl	24(%esp),%ecx		# Restore caller-saved register
	movl	20(%esp),%eax		# We are done with the exception stack
	movl	%eax,___u_xesp

	movl	%edx,%esp		# Switch back to traptime stack
	popl	%edx			# Restore registers ...
	popl	%eax
	iret				# ... and go!
	
1:	
	movl	32(%esp),%eax		# restore caller-saved register
	
	movl	16(%esp),%edx		# take trap %esp
	subl	$16,%edx		# make some space
	
	movl	24(%esp),%ecx		# stick %ecx on traptime stack
	movl	%ecx,(%edx)
	movl	28(%esp),%ecx		# stick %edx on traptime stack
	movl	%ecx,4(%edx)
	movl	12(%esp),%ecx		# stick trap %eip on traptime stack
	movl	%ecx,8(%edx)
	movl	8(%esp),%ecx		# stick trap eflags on traptime stack
	movl	%ecx,12(%edx)
		
	movl	20(%esp),%ecx		# We are done with the exception stack
	movl	%ecx,___u_xesp

	movl	%edx,%esp		# Switch back to traptime stack
	popl	%ecx			# Restore registers ...
	popl	%edx
	jmp	__DebugPfaultEntry
ENTRY(xue_fault_end)
	
	
/* On entry our stack contains an interrupt 
 * stack frame: (growing down)
 * (tt) eflags	
 * (tt) cs
 * (tt) eip	 <---%esp
 */
	
ENTRY(entrtc)	
	pushl	%es			# %es could contain $GD_ED0
	pushl	$GD_UD			# ??? should we also load %ds
	popl	%es	
	pushl	%eax			# save caller-save regs
	pushl	%edx
	pushl	%ecx
	pushl	16(%esp)		# move the (tt) %eip to top of stack
	call	_rtc_handler
	addl	$4,%esp		
	popl	%ecx
	popl	%edx
	popl	%eax
	popl	%es
	iret