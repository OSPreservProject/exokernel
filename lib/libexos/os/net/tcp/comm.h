
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

#ifndef _COMM_H_

#define _COMM_H_


void tcp_net_init(void);
void tcb_net_tcb_init(struct tcb *tcb);

#ifdef AN2
void net_copy_user_data(struct tcb *tcb, struct ae_recv *a, int datalen);
#else
void net_copy_user_data(struct tcb *tcb, char *data, int datalen);
#endif


void tcp_net_send_option(struct tcb *tcb, int flag, uint32 seqno, 
			 char *o, int olen, int len);
void tcp_net_send(struct tcb *tcb, int flag, uint32 seqno, int len);
void tcp_net_send_dst(struct tcb *tcb, int flag, uint32 seqno, int len,
		 uint16 port_dst, uint32 ip_dst, uint8 *eth_dst);


void tcp_net_poll_all(int once);
void tcp_net_tcb_init(struct tcb *tcb);

void tcp_net_close(struct tcb *tcb);

#endif
