
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

#ifndef _UDP_STRUCT_H_
#define _UDP_STRUCT_H_

#include <netinet/in.h>
#include <dpf/dpf-config.h>
#include <exos/netinet/fast_ip.h>    /* includes types */
#include <exos/netinet/fast_eth.h>
#include <sys/socket.h>

/* if INLINE_SOCKETS is defined, socket data will live inside file data
 make sure size of socket_data < FILE_DATA_SIZE 
 it is best if used without INLINE_SOCKETS,
 remains here for compatibility and debugging */
//#define INLINE_SOCKETS


#define RECVFROM_BUFSIZE 1514

#define NRUDPBUFS 8
#define DEFAULT_NRUDPBUFS 3

#define NR_RINGBUFS 64
/* should be less than NR_OPEN  */
#define NR_SOCKETS 32	/* # of sockets open at a time, total */


typedef struct ringbuf {
  struct ringbuf *next;		/* if next is NULL is not utilized */
  u_int *pollptr;		/* should point to poll */
  u_int poll;
  int n;			/* should be 1 */
  int sz;			/* should be RECVFROM_BUFSIZE */
  char *data;			/* should point to space */
  char space[RECVFROM_BUFSIZE + 2]; 
  int private;			/* if 1 means memory was malloced */
} ringbuf_t, *ringbuf_p;

struct recvfrom {
  int sockflag;               /* flag: non-blocking socket etc.. */
  unsigned short srcport;       /* in net order */
  unsigned long ipsrc;          /* in net order */

  /* HBXX leave both fields defined for easier debugging */
  /* for ring buffers */
  unsigned short ring_id;
  ringbuf_p r;

  /* for ashes */
  unsigned short nrpkt;		/* number of inserted ae_eth_pkts */
  unsigned short curpkt;		/* offset of the next ae_eth_pkts where data is recv. */
};

typedef struct recvfrom recvfrom_def;
typedef struct sockaddr_in sockaddr_def;

struct udp {
        uint16 src_port;
        uint16 dst_port;
        uint16 length;
        uint16 cksum;
};
struct eiu {
        struct eth eth;         /* ethernet header */
        struct ip ip;           /* this udp is carried on ip */
        struct udp udp;         /* standard udp */
        uint8 data[0];           /* struct hack */
};

#define DPF_PER_SOCKET 8
typedef struct socket_data {
  unsigned short port;          /* in host order */
  unsigned int demux_id;
  unsigned int aux_fids[DPF_PER_SOCKET];
  recvfrom_def recvfrom;
  sockaddr_def tosockaddr;
  sockaddr_def fromsockaddr;
  int inuse;
} socket_data_t, *socket_data_p;

void pr_recvfrom(struct recvfrom * recvfrom);
void kpr_recvfrom(struct recvfrom * recvfrom);
#endif /* _UDP_STRUCT_H_ */
