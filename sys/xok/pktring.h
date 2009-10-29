
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

#ifndef _XOK_PKTRING_H_
#define _XOK_PKTRING_H_
 
#include <xok/defs.h>
#include <xok/types.h>
#include <xok/queue.h>

#include <xok/ae_recv.h>


/* Maximum number of active pktrings supported by kernel. */

#define MAX_PKTRING_COUNT       127


/* Constants determining who owns a particular pktring entry. */

#define PKTRINGENT_OWNEDBY_APP		0
#define PKTRINGENT_OWNEDBY_KERNEL	1


typedef struct pktringent {
   struct pktringent *next;
   u_int *owner;
#ifdef KERNEL
   struct pktringent *appaddr;	/* only in kernel -- don't ref in app pktringents*/
#else
   u_int ownval;
#endif
   struct ae_recv recv;
} pktringent;


#ifdef KERNEL

/* prototypes for the kernel */
int pktring_adddpfref (int usering, int filterid);
void pktring_deldpfref (int usering, int filterid);
void pktring_handlepkt (int filterid, struct ae_recv *recv);

#endif

#endif /* _XOK_PKTRING_H_ */

