
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


#ifndef _XIO_TCP_STATS_H_
#define _XIO_TCP_STATS_H_


typedef struct {
    int         nacksnd;        /* # normal acks */
    int         nack;           /* # acks received */
    int         nvack;          /* # volunteerly acks */
    int         ndelayack;      /* # delayed acks */
    int         nackprd;        /* # predicted acks */
    int         ndataprd;       /* # predicted data */
    int         nzerowndsnt;    /* # acks advertising zero windows */
    int         nretrans;       /* # retransmissions */
    int         nethretrans;    /* # ether retransmissions */
    int         nfinsnd;        /* # fin segments */
    int         nfinrcv;        /* # fin good segments received */
    int         ndatarcv;       /* # segments with data */
    int         norder;         /* # segments in order */
    int         nbuf;           /* # segments buffered */
    int         noutspace;      /* # segments with data but no usr buffer */
    int         noutorder;      /* # segments out of order */
    int         nprobercv;      /* # window progbes */
    int         nprobesnd;      /* # window probes send */
    int         nseg;           /* # segments received */
    int         nincomplete;    /* # incomplete segments */
    int         nsynsnd;        /* # syn segments sent */
    int         nsynrcv;        /* # syn segments received */
    int         nopen;          /* # tcp_open calls */
    int         nlisten;        /* # tcp_listen calls */
    int         naccepts;       /* # tcp_accept calls */
    int         nread;          /* # tcp_read calls */
    int         nwrite;         /* # tcp_write calls */
    int         ndata;          /* # segments sent with data */
    int         nclose;         /* # tcp_close calls */
    int         ndropsnd;       /* # segments dropped when sending */
    int         ndroprcv;       /* # segments dropped when receiving */
    int         nrstsnd;        /* # reset segments send */
    int         nrstrcv;        /* # reset segments received */
    int         naccept;        /* # acceptable segments */
    int         nunaccept;      /* # unacceptable segments */
    int         nbadipcksum;    /* # segments with bad ip checksum */
    int         nbadtcpcksum;   /* # segments with bad tcp checksum */
#define NLOG2                   13      /* round up of 2 log MSS */
    int         rcvdata[NLOG2];
    int         snddata[NLOG2];
} tcp_stat;


#ifndef NOSTATISTICS
#define STINC(tcp_stat,field)		if (tcp_stat) {(tcp_stat)->field++;}
#define STDEC(tcp_stat,field)		if (tcp_stat) {(tcp_stat)->field--;}
#define STINC_SIZE(tcp_stat,field,n)	if ((tcp_stat) && (n < NLOG2)) \
					   {(tcp_stat)->field[n]++;}
#else /* NOSTATISTICS */
#define STINC(tcp_stat,field)
#define STDEC(tcp_stat,field)
#define STINC_SIZE(tcp_stat,field,n)
#endif


static inline int log2 (int size) {	\
	   int s = (size >> 1), r = 0; \
	   while (s != 0) { \
	      s = s >> 1; \
	      r++; \
	   } \
	   return r; \
	}

/* prototypes */

void xio_tcp_statprint (tcp_stat *stats);

#endif /* _XIO_TCP_STATS_H_ */

