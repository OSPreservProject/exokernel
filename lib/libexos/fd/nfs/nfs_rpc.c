
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

//#undef PRINTF_LEVEL
//#define PRINTF_LEVEL 9
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "netdb.h"
#include <string.h>
#include <assert.h>
#include <exos/signal.h>

#include "fd/proc.h"
#include "nfs_rpc.h"
#include "nfs_rpc_procs.h"
#include "nfs_xdr.h"

#include <exos/debug.h>

#include <exos/locks.h>

#include <errno.h>

#include <exos/tick.h>

/* generic_rpc_call

 returns: number of bytes received
 */

#include <xok/sysinfo.h>
#include <xok/ae_recv.h>

#define RATE (__sysinfo.si_rate)
#define TICK (__sysinfo.si_system_ticks)

extern char *__progname;

int 
p_generic_rpc_call(struct generic_server *server, struct p_rpc *prpc)
{
  int result;
  int n,i,j;
  int len;
  int xid;
  int *start;
  int *end;
  int found_total = 0;
  int retval;
  unsigned int timeout, init_timeout, max_timeout;
  struct timeval timeoutt;
  unsigned int usecs;
  /*  int retrans      = server->retrans;*/

  int number;
  struct p_rpc_rec *rw;
  /*  int retrans      = server->retrans;*/

  timeout = init_timeout = 100000  ; //server->timeo      *1000;
  max_timeout = init_timeout << 7; //NFS_MAX_RPC_TIMEOUT*10000;

  number = prpc->n;
  signals_off();

  assert(timeout < max_timeout);
  assert(init_timeout < max_timeout);


  for (timeout = init_timeout, n = 0 ; ; n++, timeout <<= 1) 
    {
      if (timeout > max_timeout) {
	printf("###\n###\n### NFS MAX TIMEOUT REACHED ON init %d to: %d max: %d %s\n",
	       init_timeout,timeout,max_timeout,
	       __progname);
	printf("TIMEOUT p_generic_rpc_call %d __sysinfo.si_rate: %d __sysinfo.si_system_ticks %lld\n",
	       n,RATE,TICK);
	timeout = init_timeout;
	errno = EIO;
	retval = -1;
	goto done;
      }

      exos_lock_get_nb(&(server->lock));
      for (i = 0; i < number; i++) {
	rw = &prpc->rw[i];
	if (rw->done == 0) {
	  struct ae_recv m = {.n = 1};
	  start = rw->ptr; 
	  end = rw->end;
	  len = ((char *) end) - ((char *) start);
	  /* 	      if (timeout != 1) printf("."); */
	  m.r[0].data = (char *)start;
	  m.r[0].sz = len;
	  for (j = 0 ; j < rw->r.n ; j++)
	    m.r[j+1] = rw->r.r[j];
	  m.n += rw->r.n;

	  result = server_sendv(server, &m);

	  if (result < 0) {
	    printf("generic_rpc_call: send error = %d errno: %d\n", 
		   result,errno);
	    retval = -1;
	    exos_lock_release(&(server->lock));
	    goto done;
	  }
	  DPRINTF(CLUHELP_LEVEL,
		  ("sending for %d, xid: %08x\n",i,rw->xid));
	  
	}
      }

      
      usecs = timeout;
      if (usecs > 1000000) {
	timeoutt.tv_sec = usecs / 1000000; timeoutt.tv_usec = usecs % 1000000;
      } else {
	timeoutt.tv_sec = 0; timeoutt.tv_usec = usecs;
      }
      
    retry:
      /* go to the top */
      result = server_select(server,&timeoutt);
      if (result == 0) {
	exos_lock_release(&(server->lock));
	yield(-1); /* so others can get lock */
	continue;
      }

      result = server_recvfrom(server,&xid,4,MSG_PEEK);

      demand(result == 4, hmm result not eq 4);
      for (i = 0; i < number; i++) {
	rw = &prpc->rw[i];
	if (rw->done == 0 && rw->xid == xid) {
	  result = server_recvfrom(server,(void *)rw->ptr, rw->size, 0);
	  DPRINTF(CLUHELP_LEVEL,("generic_rpc_call: received %d "
				 "start %08x xid %d\n",
				 result,(int)rw->start,rw->xid));
	  
	  demand(result != -1,hmm would block after a msg peek);
	  
	  rw->read = result;
	  rw->done = 1;
	  found_total++;
	  goto check;
	}
      }
      result = server_recvfrom(server,(void *)&xid, 4, 0);
#ifdef VERBOSE_DROPPING			    /* dropping result==4 messages */
      printf("result: %d\n",result);	    /* get intermingled with pipeline */
      printf("dropping xid: %d \n",xid);    /* output when this happens */
#endif
	
      demand(result == 4, hmm result not eq 4);
    check:
      if (found_total == number) {
	retval = 1;
	exos_lock_release(&(server->lock));
	goto done;
      }
      goto retry;
      
    }
  printf("*** WARNING FALLING OFF P_RPC LOOP\n");
done:
  signals_on();
  return retval;
  
}


int *
p_generic_rpc_verify(int *p)
{
  unsigned int n;
  int xid;
  xid = *p++;
  if ((n = ntohl(*p++)) != RPC_REPLY) {
    printf("generic_rpc_verify: not an RPC reply: %d\n", n);
    return NULL;
  }
  if ((n = ntohl(*p++)) != RPC_MSG_ACCEPTED) {
    printf("generic_rpc_verify: RPC call rejected: %d\n", n);
    return NULL;
  }
  switch (n = ntohl(*p++)) {
  case RPC_AUTH_NULL: case RPC_AUTH_UNIX: case RPC_AUTH_SHORT:
    break;
  default:
    printf("generic_rpc_verify: bad RPC authentication type: %d\n", n);
    return NULL;
  }
  if ((n = ntohl(*p++)) > 400) {
    printf("generic_rpc_verify: giant auth size\n");
    return NULL;
  }
  p += (n + 3) >> 2;
  if ((n = ntohl(*p++)) != RPC_SUCCESS) {
    printf("generic_rpc_verify: RPC call failed: %d, xid %d\n", n,xid);
    return NULL;
  }
  return p;
}
  

/* 
 *  generic_rpc functions 
 */


inline int *
overhead_rpc_alloc(int size) {
#if 1
  static char overhead_rpc_place[8300];	
  return (int *)overhead_rpc_place;
#else
  size+=NFS_SLACK_SPACE;		
  /* Allow for the NFS crap as well as buffer */
  return (int *)__malloc(size); 
#endif

}

inline void 
overhead_rpc_free(int *p) {
  /*   __free((void *)p);    */
}

int *
generic_rpc_header(int *p, int program, int version, int procedure, 
		   int ruid) {
  int *p1, *p2;
  int i;
  *p++ = htonl(nfs_shared_data->xid++);
  *p++ = htonl(RPC_CALL);
  *p++ = htonl(RPC_VERSION);
  *p++ = htonl(program);
  *p++ = htonl(version);
  *p++ = htonl(procedure);
  *p++ = htonl(RPC_AUTH_UNIX);
  p1 = p++;
  *p++ = htonl(CURRENT_TIME);
  //  p = xdr_encode_string(p, (char *) sys);
  *p++ = htonl(0); /* using BSD's like no machine name */
  *p++ = htonl(ruid ? __current->uid : __current->fsuid);
  *p++ = htonl(__current->egid);
  p2 = p++;
  for (i = 0; i < 16 && i < NGROUPS && __current->groups[i] != GROUP_END; i++)
    *p++ = htonl(__current->groups[i]);
  *p2 = htonl(i);
  *p1 = htonl((p - (p1 + 1)) << 2);
  *p++ = htonl(RPC_AUTH_NULL);
  *p++ = htonl(0);

  return p;
}

/* generic_rpc_call

 returns: number of bytes received,
 on error it returns -1 and sets errno.
 */
int 
generic_rpc_call(struct generic_server *server, int *start, int *end)
{
  int result;
  int n;
  int xid          = start[0];
  int len          = ((char *) end) - ((char *) start);
  unsigned int timeout, init_timeout, max_timeout;
  struct timeval timeoutt;
  unsigned int usecs;
  /*  int retrans      = server->retrans;*/

  timeout = init_timeout = 100000  ; //server->timeo      *1000;
  max_timeout = init_timeout << 7; //NFS_MAX_RPC_TIMEOUT*10000;

  signals_off();


  assert(timeout < max_timeout);
  assert(init_timeout < max_timeout);
  for (timeout = init_timeout, n = 0 ; ; n++, timeout <<= 1) 
    {
      if (timeout > max_timeout) {
	printf("###\n###\n### NFS MAX TIMEOUT REACHED ON init %d to: %d max: %d   %s\n",
	       init_timeout,timeout,max_timeout,
	       __progname);
	printf("TIMEOUT generic_rpc_call %d __sysinfo.si_rate: %d __sysinfo.si_system_ticks %lld\n",
	       n,RATE,TICK);
	timeout = init_timeout;
	errno = EIO;
	result = -1;
	goto done;
      }

      exos_lock_get_nb(&(server->lock));
      result = server_send(server, start, len);

      if (result < 0) {
	printf("generic_rpc_call: send error = %d errno %d\n", result,errno);
	exos_lock_release(&(server->lock));
	assert(0);
	break;
      }     
      
      usecs = timeout;
      if (usecs > 1000000) {
	timeoutt.tv_sec = usecs / 1000000; timeoutt.tv_usec = usecs % 1000000;
      } else {
	timeoutt.tv_sec = 0; timeoutt.tv_usec = usecs;
      }
    retry:
      result = server_select(server,&timeoutt);

      /* timed out */
      if (result == 0) {
	exos_lock_release(&(server->lock));
	yield(-1); /* so others can get lock */
	continue;
      }
      assert(result == 1);
      
      result = server_recvfrom(server,start,server->rsize + NFS_SLACK_SPACE,0);

      assert(result > 0);	/* after select!! */
      
      if (xid == start[0]) {
	DPRINTF(CLUHELP2_LEVEL, 
		("generic_rpc_call: received %d start %08x end %08x\n",
		 result,(int)start,(int)end));
	exos_lock_release(&(server->lock));
	goto done;
      } else {
	DPRINTF(CLUHELP2_LEVEL,
		("generic_rpc_call: XID mismatch expecting %08x (%08x))"
		 "got %08x\n",xid,(int)ntohl(xid),start[0]));
	goto retry;
      }
      demand(0, got to the bottom of loop);
    }

  demand(0, should never be here);
done:
  signals_on();
  return result;
}
/* generic_rpc_verify */

int *
generic_rpc_verify(int *p)
{
  unsigned int n;
  int xid;
  xid = *p++;
  if ((n = ntohl(*p++)) != RPC_REPLY) {
    printf("generic_rpc_verify: not an RPC reply: %d\n", n);
    return NULL;
  }
  if ((n = ntohl(*p++)) != RPC_MSG_ACCEPTED) {
    printf("generic_rpc_verify: RPC call rejected: %d\n", n);
    return NULL;
  }
  switch (n = ntohl(*p++)) {
  case RPC_AUTH_NULL: case RPC_AUTH_UNIX: case RPC_AUTH_SHORT:
    break;
  default:
    printf("generic_rpc_verify: bad RPC authentication type: %d\n", n);
    return NULL;
  }
  if ((n = ntohl(*p++)) > 400) {
    printf("generic_rpc_verify: giant auth size\n");
    return NULL;
  }
  p += (n + 3) >> 2;
  if ((n = ntohl(*p++)) != RPC_SUCCESS) {
    printf("generic_rpc_verify: RPC call failed: %d, xid %d\n", n,xid);
    return NULL;
  }
  return p;
}


  
