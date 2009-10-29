
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

#ifndef _TCPLIB_H_

#define _TCPLIB_H_

void 
tcp_init(void);

void *
tcp_connect(uint16 dst_port, uint32 ip_dst_addr, uint8 *eth_dst_addr,
	    uint16 src_port, uint32 ip_src_addr, uint8 *eth_src_addr);

void *
tcp_handoff(void *parent);

void
tcp_free(void *tcb);

void *
tcp_listen(int nlisten, 
	   uint16 dst_port, uint32 ip_dst_addr, uint8 *eth_dst_addr,
	   uint16 src_port, uint32 ip_src_addr, uint8 *eth_src_addr);

struct tcb *
tcp_accept(struct tcb *list_tcb);

int 
tcp_peek(struct tcb *tcb);

struct tcb *
tcp_non_blocking_accept(struct tcb *list_tcb);

void
tcp_poll(int once);

int 
tcp_write(void *handle, void *addr, size_t sz);

int 
tcp_non_blocking_write(void *handle, void *addr, size_t sz, void (*release)());

int 
tcp_peek(struct tcb *tcb);

#ifndef INPLACE
int 
tcp_read(void *handle, void *addr, size_t sz);
#else
int
tcp_read(void *handle, struct ae_recv *a, size_t sz);
#endif

int
tcp_nread(void *handle);

int 
tcp_non_blocking_close(void *handle);

int 
tcp_close(void *handle);

int
tcp_getpeername(void *handle, uint16 *port, unsigned char *ip);

int
tcp_getsockopt(void *handle, int level, int optname, void *optval, int *optlen);

int
tcp_setsockopt(void *handle, int level, int optname, const void *optval,
	       int optlen);

void 
tcp_statistics(void);

void 
tcp_set_droprate(void *handle, int rate);

#endif
