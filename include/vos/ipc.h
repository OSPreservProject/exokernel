
/*
 * Copyright 1999 MIT
 */

#ifndef __VOS_IPC_H__
#define __VOS_IPC_H__

/* distinguishes ipc from yield */
#define YIELD_WAKEUP 		0
#define IPC_WAKEUP  		1 

#ifndef __ASSEMBLER__
#include <xok/ipc.h>
#include <xok/sys_ucall.h>
#include <vos/locks.h>



/*
 * ipc_msgring_ent: an IPC message ring entry. A ring of these entries form
 * the ring structure in kernel. Upon receiving an IPC message, the ipc_msg
 * demultiplexor moves the ring pointer ahead, but does not clear the entry.
 * A pointer to the entry is passed to the request handler. After the request
 * handler is finished, ipc_clear_msgent will be called to clear the entry by
 * the libOS. A not-cleared entry can block the whole IPC message ring,
 * therefore it is crucial for the request handler not to block for long.
 */
struct ipc_msgring_ent
{
  struct ipc_msgring_ent* next;
  u_int *pollptr;       // &flag 
  u_int flag;           // initially, 0
  int n;                // 1
  int sz;               // IPC_MAX_MSG_SIZE 
  char *data;           // &space
  char space[IPC_MAX_MSG_SIZE];
};
#define IPC_MSG_EXTRAARG_START_IDX 6

static inline void
ipc_clear_msgent(struct ipc_msgring_ent *ent)
{
  if (ent != NULL)
    ent->flag = 0;
}


/* 
 * ipc_request_handler_ptr_t: ipc request handlers. Uses three registers for
 * the first three arguments, which are task number, argument 1, and argument
 * 2. The fourth argument specifies which type of IPC it is: IPC_PCT or
 * IPC_MSG. The fifth argument is the caller environment ID, and the last
 * argument is runtime data. The runtime data is NULL for IPC_PCT style IPC
 * requests, and the pointer to the IPC message entry in the message ring if
 * the request is IPC_MSG. A handler should return errors using errno. The
 * errono value will be propagated back to the caller via either messages or
 * IPC2 trap. A handler may return a second return value in the %edx register
 * as well.
 */

#define IPC_PCT		1
#define IPC_MSG		2

typedef int (*ipc_request_handler_ptr_t) (int, int, int, int, u_int, void *) 
  __attribute__ ((regparm (3)));

/* 
 * ipc_reply_handler_ptr_t: ipc reply handlers. Uses three registers for the
 * first three arguments. An IPC request does not need to have a reply
 * handler. If no such handler is defined, the returned value from the ipc
 * request, passed in either via IPC2 trap or messages, will be stored in the
 * location defined in the corresponding outstanding_ipc_t structure. 
 */
typedef int (*ipc_reply_handler_ptr_t) (int, int, int, int, u_int) 
  __attribute__ ((regparm (3)));

  
/* 
 * outstanding ipc request table: stores ipc reply handlers, ipc request
 * status, and location to place the ipc request's return value. An
 * outstanding IPC ID, of type u_int, indexes a table of outstanding_ipc_t
 * records. It is passed back to the caller after an IPC request is made. 
 * 
 * The status field is IPC_REQ_FREE for free IPC request slots in the table,
 * IPC_REQ_PENDING for outstanding requests, and IPC_REQ_ACKED for requests
 * that have been answered. If the retvalue field is NULL (i.e. caller don't
 * care about return value), status will be set to IPC_REQ_FREE immediately
 * after the request has been answered. Otherwise, ipc_clear_request must be
 * called to clear an outstanding IPC request from the table.
 */
#define MAX_OUTSTANDING_IPCS_LOG2	7
#define MAX_OUTSTANDING_IPCS    	(1<<MAX_OUTSTANDING_IPCS_LOG2)
typedef struct
{
  int reqid;	
#define IPC_REQ_FREE     -1
#define IPC_REQ_PENDING   0
#define IPC_REQ_ACKED     1
  int status;    
  int *retvalue;
  ipc_reply_handler_ptr_t h;
} outstanding_ipc_t;

extern outstanding_ipc_t outstanding_ipcs[MAX_OUTSTANDING_IPCS];

#define reqid2idx(id) (id&(MAX_OUTSTANDING_IPCS-1))
  
/*
 * ipc_clear_request: clears an outstanding request from the table of
 * outstanding IPC records. This has the effect of ignoring any future
 * response to the IPC request, if any.
 */
static inline void
ipc_clear_request(u_int id)
{
  int idx = reqid2idx(id);
  cpulock_acquire();
  outstanding_ipcs[idx].reqid = 0;
  outstanding_ipcs[idx].retvalue = 0L;
  outstanding_ipcs[idx].h = 0L;
  /* must do this last */
  outstanding_ipcs[idx].status = IPC_REQ_FREE;
  cpulock_release();
}


/* 
 * pct: sends one pct request to the given environment. pct response will be
 * handled by the reply handler given, if not null. Return value from reply
 * handler, or if the reply handler is null the return value from the pct
 * request, will be stored in the given return value storage location, if not
 * null. Returns the request id (positive int) if pct is successful. The
 * request ID is an index in the outstanding_ipcs table, where return value
 * and status of request can be found. Caller should call ipc_clear_request
 * when the request is done. If errno is set to V_WOULDBLOCK even when a
 * request id is returned, PCT cannot be handled synchronously. If PCT could
 * be handled synchronously, errno will be set to 0. Returns -1 if pct failed.
 * Errno is set to:
 *
 *   V_UNAVAIL:     environment not available for pct (SMP environment only)
 *   V_INVALID:     bad target environment
 *   V_TOOMANYIPCS: too many outstanding IPCs already
 *   V_CONNREFUSED: pct failed at the kernel level
 */
extern int
pct(int task, int arg1, int arg2, u_int extra, u_int extra_location,
    ipc_reply_handler_ptr_t h, u_int envid, int *retval);



/*
 * ipc_replymsg: sends a reply message to the destination environment. Returns
 * 0 if successful. Otherwise, return -1 with errno set to 
 * 
 *   V_IPCMSG_FULL: ring full
 *   V_INVALID:     other
 */
extern int
ipc_replymsg(u_int dest, int a1, int a2, int reqid);


/*
 * ipc_msg: sends an IPC message to the destination environment. Register the
 * reply handler and return value location for the request. Returns IPC
 * request ID if successful.  Otherwise, return -1. errno is set to
 *
 *   V_TOOMANYIPCS: too many outstanding IPCs
 *   V_IPCMSG_FULL: ring full
 *   V_INVALID:     other
 */
extern int
ipc_msg(int task, int arg1, int arg2, u_int extra, u_int extra_location,
        ipc_reply_handler_ptr_t h, u_int pid, int *retval);

/*
 * ipc_msgring_callback: routine to check if msgring is filled. If so, handle
 * IPC requests and replies. Parameter passed in is ignored. This procedure is
 * called in prologue and also registered as an extra wkup condition when
 * waiting for a wakeup predicate.
 */
extern void 
ipc_msgring_callback(u_quad_t param);


/*
 * ipc_register_handler: register an ipc request handler at the ipc number
 * given by id. Returns 0 if successful. Returns -1 otherwise. Errno is set
 * to:
 *
 *   V_INVALID: id is bad, or h is null, or ipc number conflict.
 */
extern int
ipc_register_handler(u_short id, ipc_request_handler_ptr_t h);


/*
 * ipc_unregister_handler: removes the handler with ipc number given by id.
 * Always return 0.
 */
extern int
ipc_unregister_handler(u_short id);


/*
 * ipc_sleepwait: sleep wait for an IPC result to come back. Return value is
 * the return value of the IPC request. Returns -1 if failed. errno is set to: 
 *
 *   V_UNAVAIL:    cannot make any IPC attempt (both PCT and msg failed)
 *   V_WOULDBLOCK: timedout
 *   V_INTR:	   someone canceled the IPC request while we were waiting
 */
extern int
ipc_sleepwait(int task, int arg1, int arg2, u_int extra, u_int extra_location,
              ipc_reply_handler_ptr_t h, u_int pid, u_int timeout);


#endif /* !__ASSEMBLER__ */
#endif /* __VOS_IPC_H__ */

