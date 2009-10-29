
#ifndef _XOK_IPI_H_
#define _XOK_IPI_H_

#include <xok/defs.h>
#include <xok/types.h>
#include <xok/mplock_decl.h>

/* the control messages */

#define IPI_CTRL_HALT		0
#define IPI_CTRL_TLBFLUSH	1

typedef struct ipimsg
{
  u_int ctrl;		/* control message */
  u_int payload;	/* payload */
} ipimsg_t;


#ifdef __SMP__

/* send an ipi message to the destinated CPU, returns 0 if successful, returns
 * -1 if failed. does not make any guarantees on if the destinated CPU
 * received the interrupt or not */
extern int ipi_send_message(u_short cpu, u_short ctrl, u_int payload);

/* handles IPI messages */
extern void ipi_handler();

#endif

#endif /* XOK_IPI_H */


