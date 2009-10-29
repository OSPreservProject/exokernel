
/*
 * message rings for transfering messages via kernel.
 */

#ifndef _XOK_MSGRING_H_
#define _XOK_MSGRING_H_
 
#include <xok/defs.h>
#include <xok/types.h>
#include <xok/queue.h>
#include <xok/ae_recv.h>

/* maximum number of active messages supported by kernel. */
#define IPC_MAX_MSGRING_COUNT	127

/* constants determining who owns a particular pktring entry */
#define IPC_MSGRINGENT_OWNEDBY_APP		0
#define IPC_MSGRINGENT_OWNEDBY_KERNEL		1

/* ipc message size */
#define IPC_MAX_MSG_SIZE	64

/* maximum number of scattered ptr - can only be two based on above def of
 * message size */
#define IPC_MAX_SCATTER_PTR	2

typedef struct msgringent 
{
   struct msgringent *next;
   u_int *owner;
#ifdef KERNEL
   /* only in kernel -- don't ref in app msgringents*/
   struct msgringent *appaddr;	
#else
   u_int ownval;
#endif
   struct ae_recv body;
} msgringent;


#endif /* _XOK_MSGRING_H_ */

