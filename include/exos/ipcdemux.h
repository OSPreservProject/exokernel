
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

#ifndef EXOS_IPCDEMUX_H
#define EXOS_IPCDEMUX_H

#include <exos/ipc.h>

#define IPCDEMUX_RET_NO_HAND -1 /* No handler for requested code */

typedef int (*ipcdemux_handler)();

/* XXX - remove once everyone switched to new ipc code */
void ipc_register(int code, ipcdemux_handler handler);
/* Register NULL to remove a handler.
   Handler always gets code as first argument and caller as last argument,
   thus a minimum of 2 for numargs  */
int ipcdemux_register(int code, ipcdemux_handler handler, int numargs);

#define IPC_PING            0
#define IPC_SIGNAL          1
#define IPC_EXIT            2
#define IPC_AUTH            3

/* IPC name server */
#define IPC_NS              4

/* NFS */
#define IPC_NFS_READ        5
#define IPC_NFS_WRITE       6

#define IPC_PRINT           7

#define IPC_USER            8   /* guaranteed not to be stomped on and can
				   (should?) be used by user level code as
				   generic ipc conversation code */
/* ARP TABLE */
#define IPC_ARP_ADD         9
#define IPC_ARP_DELETE     10
/* PROC TABLE */
#define IPC_PROC_SETSID    11
#define IPC_PROC_SETPGID   12
#define IPC_PROC_FORK      13
#define IPC_PROC_VFORK     14
#define IPC_PROC_EXEC      15
#define IPC_PROC_SETSTAT   16
#define IPC_PROC_REAP      17
#define IPC_PROC_SETLOGIN  18
#define IPC_PROC_CONTROLT  19
#define IPC_PROC_PTRACE    20
#define IPC_PROC_REPARENT  21

/* PAGING */
#define IPC_PAGE_REQ       22   /* caller is requesting pages from callee */

/* PTRACE */
#define IPC_PTRACE         23

/* CFFSD protected methods via user-level server */
#define IPC_CFFSD          24

/* Shared Library Server */
#define IPC_SLS            25

#endif
