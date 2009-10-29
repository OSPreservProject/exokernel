
/*
 * Copyright (C) 1998 Exotec, Inc.
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
 * associated documentation will at all times remain with Exotec, Inc..
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by Exotec, Inc. The rest
 * of this file is covered by the copyright notices, if any, listed below.
 */


#include <xok/asm.h>

	.text

ENTRY(ptrace_breakpoint_handler_s)
	movl	%edi, _pt_r_s+28	/* save regs */
	movl	$_pt_r_s, %edi
	movl	%eax, (%edi)
	movl	%ecx, 4(%edi)
	movl	%edx, 8(%edi)
	movl	%ebx, 12(%edi)
	movl	%esp, 16(%edi)
	movl	%ebp, 20(%edi)
	movl	%esi, 24(%edi)
	movl	0(%esp), %ebx		/* eip */
	movl	%ebx, 32(%edi)		/* eip */
	movl	8(%esp), %ebx		/* eflags */
	movl	%ebx, 36(%edi)		/* eflags */
	movl	4(%esp), %ebx		/* cs */
	movl	%ebx, 40(%edi)		/* cs */
	movl	%ss, 44(%edi)
	movl	%ds, 48(%edi)
	movl	%es, 52(%edi)
	movl	%fs, 56(%edi)
	movl	%gs, 60(%edi)

	movl	$_ptrace_breakpoint_handler_c, %ebx
	call	%ebx

	movl	$_pt_r_s, %edi
	movl	(%edi), %eax
	movl	4(%edi), %ecx
	movl	8(%edi), %edx
	movl	16(%edi), %esp
	movl	20(%edi), %ebp
	movl	24(%edi), %esi
	movl	32(%edi), %ebx		/* eip */
	movl	%ebx, 0(%esp)		/* eip */
	movl	36(%edi), %ebx		/* eflags */
	movl	%ebx, 8(%esp)		/* eflags */
	movl	40(%edi), %ebx		/* cs */
	movl	%ebx, 4(%esp)		/* cs */
	movl	44(%edi), %ss
	movl	48(%edi), %ds
	movl	52(%edi), %es
	movl	56(%edi), %fs
	movl	60(%edi), %gs
	movl	12(%edi), %ebx
	movl	_pt_r_s+28, %edi

	iret
