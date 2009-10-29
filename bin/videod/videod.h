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

#ifndef __MAILD_H__
#define __MAILD_H__

#include "../xio/xio_tcpcommon.h"

struct State {
  /* XXX -- keep this first for the xio demux routines */
  struct tcb st_tcb;		/* tcp transmision control block */

  enum {			/* state this connection is in */
    ST_CONNECT,			/* finish connect/accept */
    ST_HTTPGET,          	/* connection established */
    ST_SENDOK,			/* got GET request and need to send OK */
    ST_ACKWAIT,			/* out of buffer space--wait for acks */
    ST_READIN,			/* need to refill transmit buffer */
    ST_SENDING,			/* have data to send */
    ST_DONE,			/* reached EOF and no data left in buffer */
    ST_CLOSED,			/* connection is closed */
  } st_state;

  unsigned int st_sndok_seq_start; /* seq num we start sending http ok msg at */
  unsigned int st_sndok_seq_end; /* seq num we finish sending http ok msg at */

  int st_diskfd;	        /* fs fd for reading video file from */
  int st_csumfd;		/* fs fd for reading precomputed csums */

  char *st_buffer;		/* place to buffer video from disk */
#define ST_BUF_SZ (64*1024)	/* size of read buffer */
  char *st_buffer_next;		/* marks current sending pos in st_buffer */
  int st_csum_sz;		/* precomputed csum block size */
  int st_left;			/* how many bytes left to send */
  int st_bytes_read;		/* how many bytes read in last readin */
  unsigned int st_seq_start;	/* send tcp sequence no. corresponding to
				   first byte in st_buffer. */

  /* xio info */
  xio_dmxinfo_t *st_dmxinfo;	/* demultiplex info */
  xio_twinfo_t *st_twinfo;	/* ??? */
  xio_nwinfo_t *st_nwinfo;	/* network interface info */

  struct ae_recv *st_packet;	/* current incoming packet, if any */
  LIST_ENTRY(State) st_link; /* pointer to next state struct */
};

#endif /* !def(__MAILD_H__) */
