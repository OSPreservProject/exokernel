
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


#ifndef _XOK_SYSCALL_H_
#define _XOK_SYSCALL_H_

#include <xok/trap.h>  /* KERNSRV_OFFSET */

/* Trap numbers */
#define T_SYSCALL    KERNSRV_OFFSET+0x0   /* ordinary syscall int no */
#define T_YIELD      KERNSRV_OFFSET+0x2   /* int no for yield fast-path */
#define T_YIELDS     KERNSRV_OFFSET+0x3   /* int no for yield fast-path */
#define T_IPC1       KERNSRV_OFFSET+0x4   /* an IPC trap */
#define T_IPC1S      KERNSRV_OFFSET+0x5   /* call IPC1 and enter mode U_SLEEP */
#define T_IPC2       KERNSRV_OFFSET+0x6   /* another IPC trap */
#define T_IPC2S      KERNSRV_OFFSET+0x7   /* another IPC trap */
#define T_IPCAS      KERNSRV_OFFSET+0x9   /* another IPC trap */
#define T_ASHRET     KERNSRV_OFFSET+0xa   /* Return from an ash */
#define T_FASTYIELD  KERNSRV_OFFSET+0x16  /* fast yield */
#define T_IPITEST    KERNSRV_OFFSET+0x1a  /* ipi test */
#define T_FASTTRAP   KERNSRV_OFFSET+0x1b  /* trap speed test */
#define T_MAX        KERNSRV_OFFSET+0x20  /* Higher than all trap numbers */

#ifdef __SMP__     /* trap numbers for IPI */
#define T_IPI_IPITEST  	IPITRAP_OFFSET+0xa  /* IPI test */
#define T_IPI		IPITRAP_OFFSET+0xb  /* real IPI */
#endif /* __SMP__ */


#include <xok/syscallno.h>

#ifndef __ASSEMBLER__
#include <xok/types.h>
#include <xok/xoktypes.h>
#include <ubb/xntypes.h>

/* If set, called by kernel page fault code in case of user page fault
   during a syscall.  This gives the system call a chance to cleanup.
   The three arguments are: faulting va, errcode, and trapno.
   It is reset to NULL by syscall trap code before each syscall.
*/
extern void (*syscall_pfcleanup)(u_int, u_int, u_int);

/* Used by xok/sys_proto.h */
#include <ubb/root-catalogue_decl.h>
#include <ubb/ubb_decl.h>
#include <xok/ae_recv_decl.h>
#include <xok/batch_decl.h>
#include <xok/buf_decl.h>
#include <xok/capability_decl.h>
#include <xok/console_decl.h>
#include <xok/env_decl.h>
#include <xok/pktring_decl.h>
#include <xok/msgring_decl.h>
#include <xok/pmap_decl.h>
#include <xok/pxn_decl.h>
#include <xok/scode_decl.h>
#include <xok/wk_decl.h>
#ifdef __HOST__
#include <xok/trap_decl.h>
#include <xok/desc_decl.h>
#endif

#endif /* !__ASSEMBLER__ */

#endif /* !_XOK_SYSCALL_H_ */
