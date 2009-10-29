
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

/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: memset.S,v 1.2 1996/08/19 08:13:09 tholo Exp $"
#endif

ENTRY(memset)
	pushl	%edi
	pushl	%ebx
	movl	12(%esp),%edi
	movzbl	16(%esp),%eax		/* unsigned char, zero extend */
	movl	20(%esp),%ecx
	pushl	%edi			/* push address of buffer */

	cld				/* set fill direction forward */

	/*
	 * if the string is too short, it's really not worth the overhead
	 * of aligning to word boundries, etc.  So we jump to a plain
	 * unaligned set.
	 */
	cmpl	$0x0f,%ecx
	jle	L1

	movb	%al,%ah			/* copy char to all bytes in word */
	movl	%eax,%edx
	sall	$16,%eax
	orl	%edx,%eax

	movl	%edi,%edx		/* compute misalignment */
	negl	%edx
	andl	$3,%edx
	movl	%ecx,%ebx
	subl	%edx,%ebx

	movl	%edx,%ecx		/* set until word aligned */
	rep
	stosb

	movl	%ebx,%ecx
	shrl	$2,%ecx			/* set by words */
	rep
	stosl

	movl	%ebx,%ecx		/* set remainder by bytes */
	andl	$3,%ecx
L1:	rep
	stosb

	popl	%eax			/* pop address of buffer */
	popl	%ebx
	popl	%edi
	ret
