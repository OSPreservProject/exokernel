
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

        .data

        .comm   _sls_ipc1_in,4

        .comm   sls_ipc2_esp,4
        .comm   sls_ipc2_seq,4

        .text

/*
 * IPC entry point.  On entry:
 *
 *      %edi - Calling environment ID
 *      %esi - Opaque call ID, must be returned as-is
 *      %eax - Argument 1
 *      %edx - Argument 2
 *      %ecx - Argument 3
 *      %ebx - Argument 4
 */
ENTRY(sls_xue_ipc1)
        pushl   %edi
        pushl   %ebx
        call    *(_sls_ipc1_in)
        addl    $8,%esp
        int     $T_IPC2S
1:      int     $T_YIELD
        jmp     1b

/*
 * Synchronous IPC out.  On entry:
 *
 *      %eax - Argument 1
 *      %edx - Argument 2
 *      %ecx - Argument 3
 *   4(%esp) - Argument 4
 *   8(%esp) - Target environment ID
 */
ENTRY(__sls_asm_sipcout)
        pushl   %ebx                    # Save the callee-saved registers
        pushl   %ebp                    #   (as they will get trashed).
        pushl   %esi
        pushl   %edi

        movl    20(%esp),%ebx           # %ebx = arg4;
        movl    24(%esp),%edi           # %edi = destination_env;
        movl    sls_ipc2_seq,%esi       # %esi = ++ipc2_seq;
        incl    %esi
        movl    %esi,sls_ipc2_seq
        movl    %esp,sls_ipc2_esp           # Save %esp for nested IPC2 upcalls
        movl    $sls_ipc2_entry,___u_entipc2
                                        # u.u_entipc2 = &&ipc2_entry;
        int     $T_IPC1S                # Make IPC and sleep
        jmp     sls_ipc2_failed         # IPC failed

/*
 * IPC2 entry point.  (return from synchronous IPC1) On entry:
 *
 *      %edi - Calling environment ID
 *      %esi - Opaque call ID, must be returned as-is
 *      %eax - Return value
 *      %edx - Other possible return value (long long ?)
 */
        ALIGN_TEXT
sls_ipc2_entry:
        movl    sls_ipc2_esp,%ebx       # Make sure we have the right caller
        cmpl    %edi,24(%ebx)           #   (using the saved %esp, in case
        jne     1f                      #    we got nested ipc2 upcalls.)
        cmpl    %esi,sls_ipc2_seq
        jne     1f
        movl    $0,___u_entipc2         # done; block further ipc2 entries.
        movl    %ebx,%esp               # restore stack pointer
        popl    %edi                    #  and some registers
        popl    %esi
        popl    %ebp
        popl    %ebx
        ret
1:                                      # IPC2 was bogus
        movl    24(%ebx),%eax
        movl    %edi,%edx
        movl    %ebx,%ecx
        call    _sls_ipc2bogus
        int     $T_YIELDS
        jmp     1b

sls_ipc2_failed:                        # IPC failed
        movl    $0,___u_entipc2         # Prevent any further ipc2 entries.
        movl    sls_ipc2_esp,%esp           # restore stack pointer
        popl    %edi                    #  and some registers
        popl    %esi
        popl    %ebp
        popl    %ebx
        movl    $-1,%eax                # return (-1);
        ret

