
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

#include <xok/sysinfo.h>
#include <xok/sys_ucall.h>
#include <xok/ae_recv.h>
#include <xok_include/net/ether.h>

#include <vos/assert.h>


int
ae_eth_send(void *d, int sz, int netcardno)
{
  int ret = 0;
  volatile uint pending = 1;
  char filler[ETHER_MIN_LEN];
  struct ae_recv recv;

  recv.r[0].data = d;
  recv.r[0].sz = sz;
  recv.n = 1;

  assert (netcardno < XOKNET_MAXNETS);

  if (sz < ETHER_MIN_LEN) 
  {
    recv.r[1].data = filler;
    recv.r[1].sz = ETHER_MIN_LEN - sz;
    recv.n = 2;
  }

  ret = sys_net_xmit (netcardno, &recv, (uint *) &pending, 1);
  assert ((ret == 0) || (ret == -1));

  while ((ret == 0) && (pending > 0)) { sys_geteid(); }
  return ret;
}


int
ae_eth_sendv(struct ae_recv *outgoing, int netcardno)
{
  int ret = 0;
  int sz, i;
  char filler[ETHER_MIN_LEN];
  volatile uint pending = 1;

  assert (netcardno < XOKNET_MAXNETS);

  /* make sure size is >= ETHER_MIN_LEN */
  for (sz = 0, i = 0; i < outgoing->n; i++) 
  {
    sz += outgoing->r[i].sz;
  }

  if (sz < ETHER_MIN_LEN) 
  {
    outgoing->r[outgoing->n].sz = ETHER_MIN_LEN - sz;
    outgoing->r[outgoing->n].data = &filler[0];
    outgoing->n++;
  }

  ret = sys_net_xmit (netcardno, outgoing, (uint *) &pending, 1);
  assert ((ret == 0) || (ret == -1));

  while ((ret == 0) && pending > 0) { sys_geteid(); }
  return ret;
}


