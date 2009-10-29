
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


#include "stdio.h"
#include "sys/types.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "assert.h"
#include "stdlib.h"
#include "string.h"
#include "sys/stat.h"
#include "fcntl.h"
#include <sys/stat.h>
#include "unistd.h"
#include "sys/time.h"
#include "errno.h"

#include "videod.h"

#include <xok/sysinfo.h>
#include <exos/tick.h>
#include "../xio/xio_tcpcommon.h"
#include "xio_helper.h"

/***** start server specific code (state handlers) *****/

/* where we go to look for mpegs */
#define PATH_PREFIX "/usr/lib/mpeg/"

/* port we listen on */
#define SERVER_PORT 10102

/* how often, in usecs, to check for timed-out packets */
#define TIMEOUT_POLL_INTERVAL 10000 

int end_connection (struct State *st);
void clone_state (struct State *, struct State *);
void done_with_connection (struct State *st);
int handle_timewait (struct State *st);

/* 
 * following few functions are wrappers around the state struct
 * to abstract the file buffering a little bit.
 */

/* verify the buffer invariant */
void buffer_verify (struct State *st) {
  assert (st->st_buffer_next == st->st_buffer+(st->st_bytes_read-st->st_left));
}

/* create buffers associated with a connection */
void buffer_create (struct State *st) {
  st->st_buffer_next = st->st_buffer = (char *)malloc (ST_BUF_SZ);
  assert (st->st_buffer);
  st->st_left = 0;
  /* XXX -- note, the buffer invariant doesn't hold at this point */
}

/* number of bytes left in buffer */
int buffer_left (struct State *st) {
  return st->st_left;
}

/* pointer to start of 1st byte to be sent next */
char *buffer_send_next (struct State *st) {
  return st->st_buffer_next;
}

/* record the fact that a number of bytes have been sent (though not acked) */
void buffer_sent (struct State *st, int bytes) {
  st->st_left -= bytes;
  st->st_buffer_next += bytes;
  buffer_verify (st);
}

/* backup the sending position so we can retransmit */
void buffer_backup (struct State *st, int bytes) {
  st->st_left += bytes;
  st->st_buffer_next -= bytes;
  buffer_verify (st);
}

/* read-in a number of bytes */
void buffer_reload (struct State *st, int bytes) {
  st->st_left = bytes;
  st->st_buffer_next = st->st_buffer;
  buffer_verify (st);
}

/* free mem associated with a buffer */
void buffer_free (struct State *st) {
  close (st->st_diskfd);
  //  close (st->st_csumfd);
  free (st->st_buffer);
}

/* 
 * Open the input file and associated checksum file and store the
 * fd's in the state struct as well as the size of the checksum'ing
 * block size.
 */

void get_diskfd (struct State *st, char *fn) {
#if 0
  char csum_name[256];
  struct {
    char sig[22];
    unsigned int pktsz;
  } csum_header;
#endif

  st->st_diskfd = open (fn, O_RDONLY);
  assert (st->st_diskfd != -1);

  //  strncpy (csum_name, fn, 256-10);
  //  strcat (csum_name, ".csum");
  //  st->st_csumfd = open (csum_name, O_RDONLY);
  //  assert (st->st_csumfd != -1);

  //  if (read (st->st_csumfd, &csum_header, sizeof (csum_header)) != 
  //      sizeof (csum_header) || csum_header.pktsz > 1460) {
  //    fprintf (stderr, "invalid csum file %s\n", csum_name);
  //    exit (1);
  //  }
//  st->st_csum_sz = csum_header.pktsz;
}

/* 
 * Extract the filename from a non NULL-terminated buffer containing
 * an http 'get' request.
 */

void get_fn (struct State *st, char *fn1, int fn1sz, char *fn2, int fn2sz) {
#define BUFFER_SZ 80  
  char buffer[BUFFER_SZ];
  char *tok;

  strncpy (buffer, fn1, min (BUFFER_SZ,fn1sz));
  buffer[min (BUFFER_SZ, fn1sz)-1] = '\0';

  tok = strtok (buffer, " ");
  assert (strcmp ("GET", tok) == 0);
  tok = strtok (NULL, " ");
  assert (tok);

#if 0
  strncpy (fn2, PATH_PREFIX, fn2sz);
  fn2sz -= strlen (PATH_PREFIX);
  assert (fn2sz > 0);
  strncat (fn2, tok, fn2sz);
#else
  strncpy (fn2, tok, fn2sz);
#endif
}

/* SENDOK */
int send_ok (struct State *st) {
  struct stat stat_info;
  char ok_msg_out[256];
  char *ok_msg = 
    "HTTP/1.0 200 OK\n"
    "Content-length: %d\n"
    "Content-type: video/mpeg\n\n";
  int sz;

  if (fstat (st->st_diskfd, &stat_info) < 0) {
    assert (0);
  }

  sprintf (ok_msg_out, ok_msg, stat_info.st_size);
  sz = strlen (ok_msg);

  st->st_sndok_seq_start = st->st_tcb.snd_next;

  xio_tcp_prepDataPacket (&st->st_tcb, ok_msg, sz, NULL, 0, 0, -1);

  st->st_sndok_seq_end = st->st_tcb.snd_next-1;

  st->st_state = ST_ACKWAIT;
  return 1;			/* go ahead and start waiting for the ack */
}

/* READIN */
int fill_buffer (struct State *st) {
  int bytes_read;

  assert (xio_tcp_unacked (&st->st_tcb) == 0);

  bytes_read = read (st->st_diskfd, st->st_buffer, ST_BUF_SZ);
  assert (bytes_read != -1);
  st->st_bytes_read = bytes_read;

  /* remember the sequence number that the first byte in the buffer
     corresponds too */

  st->st_seq_start = st->st_tcb.snd_next;

  if (bytes_read == 0) {
    st->st_state = ST_DONE;
    return (1); /* go ahead and move to the next state */
  } else {
    buffer_reload (st, bytes_read);
    st->st_state = ST_SENDING;
    if (xio_tcp_maxsendsz (&st->st_tcb) > 0) {
      return (1);		/* start sending it */
    } else {
      return (0);		/* wait until we get some acks */
    }
  }
}

/* SEND */
int send_packet (struct State *st) {
  int bytes_to_send;
  int ret;

  /* should only get here if we have buffer to send */
  assert (buffer_left (st) > 0);

  bytes_to_send = min (buffer_left (st), xio_tcp_maxsendsz (&st->st_tcb));

  /* if we don't have any window space to send into, we go do something else */
  if (bytes_to_send == 0) {
    return 0;
  }

  ret = xio_tcp_prepDataPacket (&st->st_tcb, buffer_send_next (st), 
				bytes_to_send, NULL, 0, 0, -1);
  assert (ret);
  buffer_sent (st, bytes_to_send);

  if (buffer_left (st) == 0) {

    /* XXX -- shouldn't really wait until all data is acked,
       should switch back and forth between two buffers */

    if (xio_tcp_unacked (&st->st_tcb)) {

      /* can't punt our buffer since we may need it to retransmit
	 so don't move to readin state and instead just hang out
	 waiting for acks */

      st->st_state = ST_ACKWAIT;
      return 0;			/* block waiting for acks */
    } else {

      /* ok, all the data in the buffer has been sent and acked, so
	 go ahead and replace it with new data */

      st->st_state = ST_READIN;
      return (1);		/* go ahead and start the read */
    }

  } /* not reached if buffer_left (st) == 0 */

  /* only keep going if we can still transmit more data */

  if (xio_tcp_windowsz (&st->st_tcb) > 0) {
    return (1);
  } else {
    return (0);
  }
}

/* START STATE */
int start_accept (struct State *mainst, struct ae_recv *packet, 
		   int netcardno) {
  struct State *st;

  st = (struct State *)malloc (sizeof (struct State));
  assert (st);
  clone_state (mainst, st);
  xio_tcp_inittcb (&st->st_tcb);
  st->st_tcb.netcardno = netcardno;

  buffer_create (st);

  xio_tcp_handleListenPacket (&st->st_tcb, packet->r[0].data);

  /* we send SYN & ACK to their SYN */
  xio_tcpcommon_sendPacket (&st->st_tcb, 0);

  xio_tcp_demux_addconn (st->st_dmxinfo, (struct tcb *)st, 0);

  st->st_state = ST_CONNECT;
  st->st_seq_start = 0;

  return (0);
}  

/* CONNECT */
int finish_accept (struct State *st) {

  /* we get their ACK to our SYN */
  assert (st->st_tcb.state == TCB_ST_SYN_RECEIVED || 
	  st->st_tcb.state == TCB_ST_ESTABLISHED);
  st->st_state = ST_HTTPGET;

  return (0);
}

/* HTTPGET */
int open_client_file (struct State *st) {
#define MAX_FN 256
  char fn[MAX_FN];
  int datalen;
  char *data;

  assert (st->st_tcb.state == TCB_ST_ESTABLISHED);

  if (st->st_tcb.indata) {

    /* we got data. handle it and send a response. */
    
    datalen = xio_tcp_getLiveInDataLen (&st->st_tcb);
    data = xio_tcp_getLiveInData (&st->st_tcb);

    get_fn (st, data, datalen, fn, MAX_FN);
    get_diskfd (st, fn);

    st->st_state = ST_SENDOK;
    return (1); /* go ahead and start reading the data in */
  }

  return (0); /* go back and keep waiting for another packet */
}

/* ACKWAIT */
int ack_wait (struct State *st) {
  /* only move from this state when all the data in this state's
     buffer has been acked so we know it's safe to punt it */

  if (xio_tcp_unacked (&st->st_tcb) == 0) {
    st->st_state = ST_READIN;
    return 1;			/* go ahead and start the read */
  }

  return 0;			/* wait for next packet and hope it's an ack */
}


/* process a given series of states until we have to block */
void process_state (struct State *st) {
  int keep_going = 0;

  do {

    switch (st->st_state) {
    case ST_CONNECT: keep_going = finish_accept (st); break;
    case ST_HTTPGET: keep_going = open_client_file (st); break;
    case ST_SENDOK: keep_going = send_ok (st); break;
    case ST_READIN: keep_going = fill_buffer (st); break;
    case ST_SENDING: keep_going = send_packet (st); break;
    case ST_ACKWAIT: keep_going = ack_wait (st); break;
    case ST_DONE: keep_going = end_connection (st); break;
    case ST_CLOSED: keep_going = handle_timewait (st); break;
    default: assert (0);

    } /* switch */

    if (st->st_tcb.send_ready) {
      xio_tcpcommon_sendPacket (&st->st_tcb, 0);
    }

  } while (keep_going);
}

/* adjust the state so that it properly handles the fact that a
   timeout occurred. Basically, backup whatever state we're in
   and resend the data */

void roll_back_timedout_state (struct State *st) {
  
  switch (st->st_tcb.state) {
  case TCB_ST_ESTABLISHED: {

    /* two cases: either we're retransmitting the http ok message (which is
       not buffered but rather regenerated) or we're retransmitted buffered
       file data */

    if (SEQ_GEQ (st->st_tcb.snd_next, st->st_sndok_seq_start) &&
	SEQ_LEQ (st->st_tcb.snd_next, st->st_sndok_seq_end)) {

      /* just move back to the SENDOK state and start over */

      st->st_state = ST_SENDOK;

    } else {

      /* backup our current send pointer based on how
	 much data needs to be retransmitted */

      assert (SEQ_LEQ (st->st_seq_start, st->st_tcb.snd_next));
      st->st_state = ST_SENDING; // is this right? what states can we be in?
      buffer_backup (st, xio_tcp_retransz (&st->st_tcb));
    }
    break;
  }
  }
}

/***** start server independent code *****/

LIST_HEAD(State_list, State);
static struct State_list state_list;

/*
 * Create a new connection based on an existing one.
 */

void clone_state (struct State *old, struct State *new) {
  new->st_nwinfo = old->st_nwinfo;
  new->st_dmxinfo = old->st_dmxinfo;
  new->st_twinfo = old->st_twinfo;
  LIST_INSERT_HEAD (&state_list, new, st_link);
}

/*
 * Cleanup a connection when we're done with it.
 */

void done_with_connection (struct State *st) {
  assert (st->st_tcb.state == TCB_ST_CLOSED);
  xio_tcp_demux_removeconn (st->st_dmxinfo, &st->st_tcb);
  buffer_free (st);
  LIST_REMOVE(st, st_link);
  free (st);
}

/* CLOSED */
int handle_timewait (struct State *st) {

  /* wait till we move to the time-wait state. we have to check for this
     because we will be in this state while the tcp state first goes
     to fin-wait-2 and then finally to time-wait. */

  if  (st->st_tcb.state == TCB_ST_TIME_WAIT) {
    xio_tcp_timewait_add (st->st_twinfo, &st->st_tcb, -1);
    st->st_tcb.state = TCB_ST_CLOSED;
  }

  /* free up connection info once we've actually closed the connection,
     either by going through time-wait above or by the other side
     shutting things down on us */

  if (st->st_tcb.state == TCB_ST_CLOSED) {
    done_with_connection (st);
  }

  return (0);
}

/* DONE */
int end_connection (struct State *st) {
  /* respond to a fin or send one */
  xio_tcp_initiate_close (&st->st_tcb);

  st->st_state = ST_CLOSED;

  return (0);
}

/* 
 * Go through all state structs and see if they have any packets
 * that have timed out.
 */

void handle_timeouts (struct State *mainst) {
  int changed_state;
  struct State *st;

  /* see if we can clean up any timewaiter info */
  xio_tcpcommon_TWtimeout (mainst->st_twinfo, mainst->st_nwinfo);

  /* now check all the tcb's in use */
  for (st = state_list.lh_first; st; st = st->st_link.le_next) {
    if (xio_tcp_timedout (&st->st_tcb)) {

      changed_state = xio_tcp_timeout (&st->st_tcb);
      if (changed_state) {

	/* check if the connection timed-out */
	
	if (xio_tcp_closed (&st->st_tcb)) {
	  printf ("connection timed out...\n");
	  done_with_connection (st);
	}

	/* send any packets xio may have prep'ed for us (to handle
	   connect/disconnect timeouts) */

	xio_tcpcommon_sendPacket (&st->st_tcb, 0);

	/* backup the state so that we will re-create the data
	   that needs to be retransmitted */

	roll_back_timedout_state (st);

	/* and go ahead and start doing this */
	process_state (st);
      }

    } /* if timed out */

  } /* for each tcb */

}

void main () {
  struct State mainst;
  struct State *st;
  struct ae_recv *packet = NULL;
  int netcardno;
  u_quad_t last_handle_timeout = __sysinfo.si_system_ticks;

  xio_init (&mainst.st_dmxinfo, &mainst.st_twinfo, &mainst.st_nwinfo);
  xio_listen (SERVER_PORT, mainst.st_nwinfo, mainst.st_dmxinfo);

  LIST_INIT (&state_list);

  while (1) {

    if (last_handle_timeout < 
	__sysinfo.si_system_ticks + __usecs2ticks (TIMEOUT_POLL_INTERVAL)) {
      /* time to check tcb's for timeouts */
      handle_timeouts (&mainst);
      last_handle_timeout = __sysinfo.si_system_ticks;
    }

    xio_tcp_waitforChange (mainst.st_nwinfo, TIMEOUT_POLL_INTERVAL);

    if (!xio_net_wrap_checkforPacket (mainst.st_nwinfo)) {
      continue;			/* timed out, no packet */
    }

    packet = xio_poll_packet (mainst.st_nwinfo, mainst.st_dmxinfo);

    /* check if this packet is for an established connection */
    st = (struct State *)xio_tcp_demux_searchconns (mainst.st_dmxinfo, 
						    packet->r[0].data);
    if (!st) {

      /* see if the packet is a 'connect' to our 'listen/accept' */
      if (xio_tcpcommon_findlisten (mainst.st_dmxinfo, packet->r[0].data,
				    mainst.st_nwinfo, &netcardno)) {
	start_accept (&mainst, packet, netcardno);

	/* hmm, maybe a time-wait packet? */
      } else if (xio_tcpcommon_handleTWPacket (mainst.st_twinfo,
					       packet->r[0].data,
					       mainst.st_nwinfo)) {
	/* nothing to do */
      } else {

	/* junk it */
	xio_tcpcommon_handleRandPacket (packet->r[0].data, mainst.st_nwinfo);
      }
    } else {
      /* the packet is for the connection described by 'st' */

      xio_tcp_handlePacket (&st->st_tcb, packet->r[0].data, 0);

      st->st_packet = packet;

      /* is the other side trying to close things down on us? If so,
         pretend we've sent all the data and move to the done state. */

      if (st->st_tcb.state == TCB_ST_CLOSE_WAIT) {
	st->st_state = ST_DONE;
      }

      /* do the action associated with this state and move to the next state */
      process_state (st);

    } /* if (!st) */

    /* give the packet back to the receive ring */
    if (packet) {
      xio_net_wrap_returnPacket (mainst.st_nwinfo, packet);
      packet = NULL;
    }

  } /* while (1) */

} /* main */
