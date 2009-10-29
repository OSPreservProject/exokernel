
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

#include "assert.h"
#include <exos/debug.h>

#include "tcp_socket.h"
#include "tcp_handoff.h"

#include <exos/process.h>
#include <exos/vm-layout.h>

extern void pr_filp(struct file *, char *label);
static void
tcb_print(struct tcb *tcb)
{
    printf("tcb %p (%d): una %u snxt %u smax %u swnd %u wl1 %u wl2 %u rnxt %u rwnd %u \n", 
	   tcb,
	   tcb->state, 
	   tcb->snd_una, tcb->snd_next, tcb->snd_max, tcb->snd_wnd, 
	   tcb->snd_wls, tcb->snd_wla,
	   tcb->rcv_next, tcb->rcv_wnd); 
}

static int
handoff_exec_handler(u_int k, int envid, int execonly);

static int
search_n_handoff(int parent);


void 
init_handoff(void) {
  int status;
  int i;
  printf("INIT_HANDOFF pid %d\n",getpid());

  status = fd_shm_alloc(FD_SHM_OFFSET + TCP_SOCKET_TYPE + 125 ,
			(sizeof(struct tcb_handoff)),
			(char *)TCB_SHARED_REGION);
  
  if (status == -1) assert(0);
  tcb_handoff = (struct tcb_handoff *) TCB_SHARED_REGION;
  printf("TCB_HANDOFF ADDR: %p size: %d\n",tcb_handoff, (sizeof(struct tcb_handoff)));
  if (status) {
    printf("clearing handoff region\n");
    /* not used before */
    for(i=0;i<MAXHANDOFF;i++) tcb_handoff->inuse[i] = 0;
  } else {
    printf("mapped handoff region\n");
  }
  
  OnExec(handoff_exec_handler);
  search_n_handoff(0);
  for(i=0;i<MAXHANDOFF;i++) tcb_handoff->inuse[i] = 0;
}


static int
handoff_exec_handler(u_int k, int envid, int execonly) {
  printf("HANDOFF_EXEC_HANDLER pid: %d, execonly %d\n",getpid(),execonly);
  if (execonly) 
    return search_n_handoff(1);
  else
    return 0;
}

static struct tcb * 
puttcb(struct tcb *tcb) {
  int i;
  for( i = 0; i < MAXHANDOFF ; i++ ) {
    if (tcb_handoff->inuse[i] == 0) {
      printf("FOUND SLOT %d addr: %p\n",i, &tcb_handoff->handoff[i]);
      tcb_handoff->inuse[i] = 1;
      tcb_handoff->handoff[i] = *tcb;
      return &tcb_handoff->handoff[i];
    }
  }
  demand(0,could not find slot);
  return (struct tcb *)0;
}

static int
search_n_handoff(int parent) {
  int fd;
  struct file *filp;
  struct tcp_socket_data *sock;
  struct tcb *tcb_tmp;
  extern struct tcb *tcp_handoff(struct tcb *parent);
  extern void tcp_free(struct tcb *tcb);


  printf("start search_n_handoff: %d\n",parent);
  for(fd = 0; fd < NR_OPEN ; fd++) {
    if (__current->fd[fd] != NULL) {
      filp = __current->fd[fd];
      if (filp->op_type == TCP_SOCKET_TYPE) {
	if (parent) {
	  printf("SENDING fd: %d\n",fd);
	  pr_filp(filp,"SENDING FILP");
	  /* put the tcb in our shared table */
	  sock = (struct tcp_socket_data *) &filp->data;
	  printf("SOCK addr %p\n",sock);
	  /* now sock->tcb will point to a shared location */
	  demand(sock->tcb, bogus tcb);
	  printf("\ntcb before\n");
	  tcb_print(sock->tcb);
	  tcb_tmp = sock->tcb;
	  sock->tcb = puttcb(sock->tcb);
	  printf("\ntcb after\n");
	  tcb_print(sock->tcb);
	  tcp_free(tcb_tmp);
	  printf("--\n");
	} else {
	  printf("RECEIVING fd: %d\n",fd);
	  pr_filp(filp,"RECEIVING FILP");

	  /* grab the tcb from the shared table */
	  sock = (struct tcp_socket_data *) &filp->data;
	  printf("SOCK addr %p\n",sock);
	  /* now sock->tcb will point to a local structure */
	  printf("\ntcb before\n");
	  tcb_print(sock->tcb);
	  printf("\ntcb handoff\n");
	  sock->tcb = tcp_handoff(sock->tcb);
	  printf("\ntcb after\n");
	  tcb_print(sock->tcb);
	  printf("--\n");
	}
      }
    }
  }
  printf("done search_n_handoff %d\n",parent);
  return 0;
}
