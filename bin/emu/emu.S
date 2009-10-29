
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
#include <xok/mmu.h>
#define PSL_T	0x00000100      /* trap flag */
#define PSL_NT	0x00004000      /* nested task */
	.text


/* Standard stack: (openbsd)
	...
	args to sysscall
	return address from OpenBSD stub
	flags, from int
	return cs, from int
	return ip (to OpenBSD stub), from int
*/
/* Indirect syscal stack:
	...
	args to syscall
	return address from OpenBSD stub
	(unknown word)
	real syscall number
	flags, from int
	return cs, from int
	return ip (to OpenBSD stub), from int
*/
/* Standard stack: (386bsd)
	...
	args to sysscall
	return address from 386BSD stub
	flags, from int
	return cs, from int
	return ip (to the lcall instruction), from int
	error code, from int
*/
	

ENTRY(asm_start_386bsd)
	addl	$4, %esp	/* skip error code */
	pushl	%ebx		/* skip the lcall instruction on return */
	movl	4(%esp), %ebx
	addl	$7, %ebx
	movl	%ebx, 4(%esp)
	popl	%ebx

ENTRY(asm_start)
	movl	%edi, _r_s		/* save regs */
	movl	$_r_s, %edi
	movl	%esi, 4(%edi)
	movl	%ebp, 8(%edi)
	movl	%esp, 12(%edi)
	movl	%ebx, 16(%edi)
	movl	%edx, 20(%edi)
	movl	%ecx, 24(%edi)
	movl	%eax, 28(%edi)
	movl	$_i_s, %edi
	movl	12(%esp), %ebx		/* save OBSD return address */
	movl	%ebx, 12(%edi)
	movl	8(%esp), %ebx		/* save ip, cs, flags from int */
	movl	%ebx, 8(%edi)
	movl	4(%esp), %ebx
	movl	%ebx, 4(%edi)
	movl	0(%esp), %ebx
	movl	%ebx, 0(%edi)
	movl	_r_s, %edi
	addl	$16, %esp		/* remove the four words from stack */

#if 0	
	pushal
	push	%eax
	movl	$_count_pre, %ebx
	call	%ebx
	addl	$4, %esp
	popal
#endif	
		
	movl	$_handlers, %ebx	/* get address of array of handlers */
	imull	$4, %eax
	
	addl	%eax, %ebx
	call	(%ebx)			/* call handler */

#if 0
	pushal
	movl	$_count_post, %ebx
	call	%ebx
	popal
#endif

	movl	$_i_s,%edi
	subl	$16, %esp		/* make space for int data on stack */
	movl	12(%edi), %ebx		/* load OBSD return address */
	movl	%ebx, 12(%esp)
	movl	4(%edi), %ebx		/* load ip, cs for int */
	movl	%ebx, 4(%esp)
	movl	0(%edi), %ebx
	movl	%ebx, 0(%esp)
	
	movl	8(%edi), %ebx		/* move flags into ebx */
	/* XXX not always an error if eax == -1 */
	cmpl	$-1, %eax		/* if eax == -1 */
	je	err

	andl	$~FL_CF, %ebx		/* clear carry flag */
	movl	%ebx, 8(%esp)		/* move flags onto stack */
	movl	$_r_s, %edi

	movl	4(%edi), %esi
	movl	8(%edi), %ebp
	movl	12(%edi), %esp
	movl	16(%edi), %ebx
	movl	20(%edi), %edx
	movl	24(%edi), %ecx
	movl	0(%edi), %edi		/* reload regs */
	iret

err:
	orl	$FL_CF, %ebx		/* set carry flag */
	movl	%ebx, 8(%esp)		/* move flags onto stack */
	movl	_errno, %eax		/* eax = errno */
	movl	$_r_s, %edi
	movl	4(%edi), %esi
	movl	8(%edi), %ebp
	movl	12(%edi), %esp
	movl	16(%edi), %ebx
	movl	20(%edi), %edx
	movl	24(%edi), %ecx
	movl	0(%edi), %edi		/* reload regs */
	iret
ENTRY(asm_stop)
	
ENTRY(asm_debug_handler)
	movl	%esp, _dargs
	addl	$4, _dargs
	call	_debug_handler_pre	/* executed before handler */
	
	movl	$_dret_addrs, %edi	/* save and switch ret addr */
	sall	$2, _dcount
	addl	_dcount, %edi
	sarl	$2, _dcount
	incl	_dcount
	popl	0(%edi)

	movl	$_real_handlers, %edi	/* call handler */
	movl	$_r_s, %edx
	movl	28(%edx), %edx
	sall	$2, %edx
	addl	%edi, %edx
	movl	0(%edx), %edx
	call	%edx
	decl	_dcount			/* reset return address */
	movl	$_dret_addrs, %edi
	sall	$2, _dcount
	addl	_dcount, %edi
	sarl	$2, _dcount
	pushl	0(%edi)

	pushal
	call	_debug_handler_post	/* executed after handler */
	popal
	ret

ENTRY(__syscallx)
	popl	_scallx_retaddr		/* ret addr */
	popl	%ecx			/* real syscall # */
	popl	_scallx_pad		/* quad padding */
	movl	%ecx, _scallx_scnum	/* save for later */
	movl	$_r_s, %edx		/* r_s.eax = scnum */
	movl	%ecx, 28(%edx)
	sall	$2, %ecx
	addl	$_handlers, %ecx
	call	(%ecx)			/* call the real syscall */
	pushl	_scallx_scnum
	pushl	_scallx_pad

	pushl	_scallx_retaddr		/* return to caller */
	ret

ENTRY(asm_start2)
	movl	%edi, _r_s
	movl	$_r_s, %edi
	movl	%esi, 4(%edi)
	movl	%ebp, 8(%edi)
	movl	%esp, 12(%edi)
	movl	%ebx, 16(%edi)
	movl	%edx, 20(%edi)
	movl	%ecx, 24(%edi)
	movl	%eax, 28(%edi)

	popl	%ebx
	popl	%ecx
	pushfl
	andb    $~((PSL_T|PSL_NT)>>8),1(%esp)
	pushl	%ecx
	pushl	%ebx
	movl	%esp, 12(%edi)
	
	movl	$_i_s, %edi
	movl	12(%esp), %ebx		/* save OBSD return address */
	movl	%ebx, 12(%edi)
	movl	8(%esp), %ebx		/* save ip, cs, flags from int */
	movl	%ebx, 8(%edi)
	movl	4(%esp), %ebx
	movl	%ebx, 4(%edi)
	movl	0(%esp), %ebx
	movl	%ebx, 0(%edi)
	movl	_r_s, %edi
	addl	$16, %esp		/* remove the four words from stack */
	movl	$_handlers, %ebx	/* get address of array of handlers */

	imull	$4, %eax
	
	addl	%eax, %ebx
	call	(%ebx)			/* call handler */

	movl	$_i_s,%edi
	subl	$16, %esp		/* make space for int data on stack */
	movl	12(%edi), %ebx		/* load OBSD return address */
	movl	%ebx, 12(%esp)
	movl	4(%edi), %ebx		/* load ip, cs for int */
	movl	%ebx, 4(%esp)
	movl	0(%edi), %ebx
	movl	%ebx, 0(%esp)
	
	movl	8(%edi), %ebx		/* move flags into ebx */
	cmpl	$-1, %eax		/* if eax == -1 */
	je	err2

	andl	$~FL_CF, %ebx		/* clear carry flag */
	movl	%ebx, 8(%esp)		/* move flags onto stack */
	movl	$_r_s, %edi

	movl	4(%edi), %esi
	movl	8(%edi), %ebp
	movl	12(%edi), %esp
	movl	16(%edi), %ebx
	movl	20(%edi), %edx
	movl	24(%edi), %ecx
	movl	0(%edi), %edi		/* reload regs */
	iret

err2:
	orl	$FL_CF, %ebx		/* set carry flag */
	movl	%ebx, 8(%esp)		/* move flags onto stack */
	movl	_errno, %eax		/* eax = errno */
	movl	$_r_s, %edi
	movl	4(%edi), %esi
	movl	8(%edi), %ebp
	movl	12(%edi), %esp
	movl	16(%edi), %ebx
	movl	20(%edi), %edx
	movl	24(%edi), %ecx
	movl	0(%edi), %edi		/* reload regs */
	iret
ENTRY(asm_stop2)