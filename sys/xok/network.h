
/*
 * Copyright (C) 1998 Massachusetts Institute of Technology 
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


#ifndef _XOK_NETWORK_H_
#define _XOK_NETWORK_H_

/* generic network card interface (combined with pkt.h) */

#include <xok/ae_recv.h>
#include <xok_include/net/ether.h>
#include <xok/capability.h>
#include <xok/mplock.h>

#define XOKNET_MAXNETS	8

/* card types */

#define XOKNET_DE	1
#define XOKNET_ED	2
#define XOKNET_LOOPBACK	3
#define XOKNET_OSKIT	4


struct network
{
  void *cardstruct;		/* pointer to the kernel's card structure */
#define XOKNET_NAMEMAX 16
  char cardname[XOKNET_NAMEMAX]; /* name for card */
  short in_intr;		/* in interrupt handler */
  short intr_pending;		/* pending interrupts */
  int cardtype;			/* type of network card */
  int inited;			/* is card initialized? */
  int netno;			/* network number for this card */
  int irq;			/* IRQ for the card */
  cap netcap;			/* capability covering this interface */
  u_int8_t ether_addr[ETHER_ADDR_LEN];	/* ethernet address */
  u_int64_t intrs;		/* interrupts taken since boot */
  u_int64_t rxintrs;		/* rcv interrupts taken since boot */
  u_int64_t txintrs;		/* xmit interrupts taken since boot */
  u_int64_t otherintrs;		/* other interrupts taken since boot */
  u_int64_t xmits;		/* packets transmitted since boot */
  u_int64_t rcvs;		/* packets received since boot */
  u_int64_t discards;		/* packets discarded since boot */
  u_int32_t inerrs;		/* packet reception errors */
  u_int32_t outerrs;		/* packet transmission errors */
  int (*xmitfunc)(void *, struct ae_recv *, int);  /* send function */
  struct kspinlock slock; 	/* queue lock for this device */
};


struct network * xoknet_getstructure (int cardtype, int (*xmitfunc)(void *, struct ae_recv *, int), void *construct);

void loopback_init ();

#endif /* !_XOK_NETWORK_H_ */

