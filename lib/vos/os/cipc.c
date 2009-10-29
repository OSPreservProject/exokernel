
/*
 * Copyright MIT 1999
 */

#include <stdio.h>
#include <signal.h>
#include <string.h>

#include <xok/sysinfo.h>
#include <xok/sys_ucall.h>
#include <xok/ipc.h>

#include <vos/ipc-services.h>
#include <vos/errno.h>
#include <vos/proc.h>
#include <vos/ipc.h>
#include <vos/malloc.h>
#include <vos/cap.h>
#include <vos/locks.h>
#include <vos/wk.h>

#include <vos/kprintf.h>



/***********************************************
 **     Standard demultiplexing routines      **
 ***********************************************/


/* table of outstanding IPC's */
outstanding_ipc_t outstanding_ipcs[MAX_OUTSTANDING_IPCS];

/* table of registered IPC handlers */
ipc_request_handler_ptr_t ipc_request_handlers[MAX_IPC_HANDLERS];


static inline int
ipc1_demultiplex
  (int task, int arg1, int arg2, int ipctype, u_int caller, void* data, int* r2)
{
  int r;

  if (task < MIN_IPC_NUMBER || task > MAX_IPC_NUMBER || 
      ipc_request_handlers[task] == 0L)
    RETERR(V_PROGUNAVAIL);

  errno = 0;
  r = (*(ipc_request_handlers[task]))(task,arg1,arg2,ipctype,caller,data);
  asm volatile ("\tmovl %%edx,%0" : "=g" (*r2));
  return r;
}

static inline void
ipc2_demultiplex
  (int arg0, int arg1, int reqid, int ipctype, u_int caller)
{
  int idx = reqid2idx(reqid);

  if (idx >= 0 && idx < MAX_OUTSTANDING_IPCS &&
      outstanding_ipcs[idx].status != IPC_REQ_FREE &&
      outstanding_ipcs[idx].reqid == reqid)
  {
    if (outstanding_ipcs[idx].h != 0)
    {
      int r;
      r=(*(outstanding_ipcs[idx].h))(arg0,arg1,reqid,ipctype,caller);
      
      if (outstanding_ipcs[idx].retvalue != NULL)
      {
	*(outstanding_ipcs[idx].retvalue) = r;
	outstanding_ipcs[idx].status = IPC_REQ_ACKED;
      }
      else 
	outstanding_ipcs[idx].status = IPC_REQ_FREE;
    }
    else
    {
      if (outstanding_ipcs[idx].retvalue != NULL)
      {
	*(outstanding_ipcs[idx].retvalue) = arg0;
	outstanding_ipcs[idx].status = IPC_REQ_ACKED;
      }
      else 
	outstanding_ipcs[idx].status = IPC_REQ_FREE;
    }
  }
}


static int ipc_next_reqid = 0;
static inline int 
ipc_get_reqid(ipc_reply_handler_ptr_t h, int *retval)
{
  int index;
 
  for(index = 1; index < MAX_OUTSTANDING_IPCS; index++)
  {
    if (outstanding_ipcs[index].status == IPC_REQ_FREE)
    {
      outstanding_ipcs[index].reqid = 
	((ipc_next_reqid++)<<(1+MAX_OUTSTANDING_IPCS_LOG2))|index;
      outstanding_ipcs[index].status = IPC_REQ_PENDING;
      outstanding_ipcs[index].h = h;
      outstanding_ipcs[index].retvalue = retval;
      return outstanding_ipcs[index].reqid;
    }
  }
  return -1;
}




/**************************
 **   PCT definitions    **
 **************************/


#define PCT_RET2(r1,r2) \
{ \
  asm volatile ("\tmovl %0, %%edx" :: "r" (r2)); \
  return r1; \
}

typedef int (*ipc1_handler_ptr_t) (int, int, int, u_int)
    __attribute__ ((regparm (3)));

typedef int (*ipc2_handler_ptr_t) (int, int, int, u_int)
    __attribute__ ((regparm (3)));


/* 
 * these are handlers written in asm that calls actual handlers after saving
 * states, etc.
 */
extern void ipc1_entry(void);
extern void ipc2_entry(void);


/* default handlers should be assigned to these */
extern ipc1_handler_ptr_t ipc1_in;
extern ipc2_handler_ptr_t ipc2_in;


/* make local IPC calls via IPC1 */
extern int 
__asm_ipc_local (int task, int arg1, int arg2, int num_xtrargs, int reqid, 
                 u_int envid, char* arg_location) 
  __attribute__ ((regparm (3)));



/* 
 * default ipc1 demultiplexer called from the asm routine ipc1_entry.
 * convention: arg0 is the task number. return value goes to arg0.
 *             errno goes to arg 1.
 */
static int default_ipc1(int,int,int,u_int) __attribute__ ((regparm (3)));
static int
default_ipc1(int task, int arg1, int arg2, u_int caller) 
{
  int r2;
  int r = ipc1_demultiplex(task, arg1, arg2, IPC_PCT, caller, 0L, &r2);

  if (r < 0)
    PCT_RET2(-1,errno)
  else
    PCT_RET2(r,r2);
}



/* 
 * default ipc2 demultiplexer called from the asm routine ipc2_entry.
 * convention: return value in arg0, errno, if any, in arg1
 */
static int default_ipc2(int,int,int,u_int) __attribute__ ((regparm (3)));
static int
default_ipc2(int arg0, int arg1, int reqid, u_int caller)
{
  ipc2_demultiplex(arg0, arg1, reqid, IPC_PCT, caller);
  /* 
   * must return 0 here so when returning from asm_local we know if ret value
   * is negative, kernel messed up
   */
  return 0;
}



/* making pct call */
int
pct(int task, int arg1, int arg2, u_int extra, u_int extra_location, 
    ipc_reply_handler_ptr_t h, u_int envid, int *retval)
{
  int r=0;
  struct Env* e;
  int reqid;
  
  e = env_id2env(envid, &r);
 
  if (r < 0)
    RETERR(V_INVALID);

#ifdef __SMP__
  if (e->env_cur_cpu != -1)
  {
    RETERR(V_UNAVAIL);
  }
#endif

  if ((reqid = ipc_get_reqid(h, retval)) < 1)
    RETERR(V_TOOMANYIPCS);

  errno = 0;
  r = __asm_ipc_local(task,arg1,arg2,extra,reqid,envid,(void*)extra_location);
 
  if (r < 0 && errno != V_WOULDBLOCK) /* trap to kernel failed */
  {
    ipc_clear_request(reqid);
    RETERR(V_CONNREFUSED);
  }

  return reqid;
}



/* making an ipc call and wait for result to come back */
int
ipc_sleepwait(int task, int arg1, int arg2, u_int extra, u_int extra_location,
              ipc_reply_handler_ptr_t h, u_int envid, u_int timeout)
{
  int retval,r;

  errno = 0;
  r = pct(task, arg1, arg2, extra, extra_location, h, envid, &retval);

  if (r < 0)
  {
    r = ipc_msg(task, arg1, arg2, extra, extra_location, h, envid, &retval);
    if (r < 0)
      RETERR(V_UNAVAIL);
  }

  /* r is now a request id. we need to wait for status of ipc request to
   * change, or a timeout... 
   * 
   * a few possibilities: 
   *
   *   1. waken up because of timeout 
   *   2. waken up by someone (could be ipc2, or just a yield)
   *   3. waken up because status changed to IPC_REQ_ACKED.
   *
   * XXX - if we get wakenup to deal with a packet, we dont want to go back
   * waiting again for the change to status...
   */
 
  {
    struct wk_term t[32];
    int sz = 0;
    int wr;
    int idx = reqid2idx(r);
    uint64 now;
    
again:
    now = __sysinfo.si_system_ticks;

    /* wait until status changed to pending, or ... */
    sz = wk_mkcmp_neq_pred
      (&t[0], &(outstanding_ipcs[idx].status), IPC_REQ_PENDING, CAP_USER);
   
    /* if someone canceled on us */
    sz = wk_mkop (sz, t, WK_OR);
    sz = wk_mkcmp_neq_pred
      (&t[sz], &(outstanding_ipcs[idx].reqid), r, CAP_USER);

    wr = wk_waitfor_timeout(&t[0], sz, timeout, envid);

    if (outstanding_ipcs[idx].reqid != r) 
      /* ipc was canceled */ 
      RETERR(V_INTR)

    else if (wr < 0 && errno == V_WAKENUP) 
      // did not timed out, someone just woke us up
    {

      if (outstanding_ipcs[idx].status == IPC_REQ_PENDING)
	/* still pending, someone woke us up by accident */
      {
	if (timeout != 0 && __sysinfo.si_system_ticks - now > timeout)
	{
          ipc_clear_request(r);
	  RETERR(V_WOULDBLOCK);
	}
	else
	{
	  if (timeout != 0)
	  { 
	    timeout = timeout - (__sysinfo.si_system_ticks-now); 
	    if (timeout == 0) 
	    {
	      ipc_clear_request(r);
	      RETERR(V_WOULDBLOCK);
	    }
	  }
	  goto again;
	}
      } 
      
      else
      {
	/* request finished */
	ipc_clear_request(r);
	return retval;
      }
    }

    else if (wr < 0 && errno == V_WOULDBLOCK)
      /* woke up from timeout */
    {
      ipc_clear_request(r);
      RETERR(V_WOULDBLOCK);
    }

    else 
      /* woke up because status changed */
    {
      ipc_clear_request(r);
      return retval;
    }
  }
}




/***********************************
 **    IPC Message definitions    **
 ***********************************/

#define min(a,b) ((a)<(b))?(a):(b)

#define DEFAULT_MSGRING_SZ 16
#define IPC_MSG_REQUEST  0
#define IPC_MSG_REPLY    1

struct ipc_msgring_ent* ipc_msgring = 0L;


int
ipc_replymsg(u_int envid, int a1, int a2, int reqid)
{
  int mesg[5];
  int r;

  struct Env *e = env_id2env(envid, &r);
  if (r<0)
    RETERR(V_INVALID);

  mesg[0] = IPC_MSG_REPLY;
  mesg[1] = (int) getpid();
  mesg[2] = a1;
  mesg[3] = a2;
  mesg[4] = reqid;
  
  r = sys_ipc_sendmsg(CAP_USER,e->env_id,5*sizeof(int),(unsigned long)&mesg[0]);
  
  if (r == -E_MSGRING_FULL)
    RETERR(V_IPCMSG_FULL)
  else if (r< 0)
    RETERR(V_INVALID)
  return 0;
}


int
ipc_msg(int task, int arg1, int arg2, u_int extra, u_int extra_location, 
        ipc_reply_handler_ptr_t h, u_int envid, int *retval)
{
  int r=0;
  struct Env* e;
  int reqid;
  char mesg[IPC_MAX_MSG_SIZE];

  e = env_id2env(envid, &r);
 
  if (r < 0)
    RETERR(V_INVALID);

  if ((reqid = ipc_get_reqid(h, retval)) < 1)
    RETERR(V_TOOMANYIPCS);

  ((u_int*)mesg)[0] = IPC_MSG_REQUEST;
  ((u_int*)mesg)[1] = getpid();
  ((int*)  mesg)[2] = task;
  ((int*)  mesg)[3] = arg1;
  ((int*)  mesg)[4] = arg2;
  ((int*)  mesg)[5] = reqid;

  if (extra > 0)
  {
    int extrasz = min(IPC_MAX_MSG_SIZE-IPC_MSG_EXTRAARG_START_IDX*sizeof(int),
	              extra*sizeof(u_int));
    memmove(&((int*) mesg)[IPC_MSG_EXTRAARG_START_IDX], 
	    (void*)extra_location, extrasz);
  }

  r = sys_ipc_sendmsg(CAP_USER, e->env_id, 
                      IPC_MAX_MSG_SIZE, (unsigned long)&mesg[0]);

  if (r == -E_MSGRING_FULL)
  {
    ipc_clear_request(reqid);
    RETERR(V_IPCMSG_FULL);
  }
  else if (r < 0)
  {
    ipc_clear_request(reqid);
    RETERR(V_INVALID);
  }
  return reqid;
}


static void
ipc_setup_msgring()
{
  int i=0;
  struct ipc_msgring_ent* m;
  struct ipc_msgring_ent* m_prev = 0L;

  for(i=0; i<DEFAULT_MSGRING_SZ; i++)
  {
    m = (struct ipc_msgring_ent*) malloc(sizeof(struct ipc_msgring_ent));
    if (m == 0L)
    {
      if (ipc_msgring != 0L) 
	m_prev->next = ipc_msgring->next;
      break;
    }
    
    if (ipc_msgring == 0L)
      ipc_msgring = m;

    m->pollptr = &(m->flag);
    m->flag = 0;
    m->n = 1;
    m->sz = IPC_MAX_MSG_SIZE;
    m->data = &(m->space[0]);

    if (m_prev != 0L)
      m_prev->next = m;
    m_prev = m;
  }

  if (m != 0L) 
    m->next = ipc_msgring;

  if (ipc_msgring != 0L) 
  {
    if (sys_msgring_setring((struct msgringent *) ipc_msgring) < 0) 
      printf("warning: cannot setup message ring for ipc use.\n");
  }
  else
    printf("warning: cannot setup message ring for ipc use.\n");
}


static void
ipc_setup_msgring_afterfork()
{
  int i=0;
  struct ipc_msgring_ent* m = ipc_msgring;
  
  for(i=0; i<DEFAULT_MSGRING_SZ; i++)
  {
    m = m->next;
    m->flag = 0;
  }

  if (ipc_msgring != 0L) 
  {
    if (sys_msgring_setring((struct msgringent *) ipc_msgring) < 0) 
      printf("warning: cannot setup message ring for ipc use.\n");
  }
  else
    printf("warning: cannot setup message ring for ipc use.\n");
}


static int
ipc_msgring_pred_constr(struct wk_term *t, u_quad_t param)
{
  return wk_mkcmp_neq_pred(t, &ipc_msgring->flag, 0, CAP_USER);
}


void
ipc_msgring_callback(u_quad_t param)
{
  u_int msg_type;
  u_int caller;
  int reqid, r;
  int task, arg1, arg2;
  extern void flush_io();

  /* !!! here we increment and decrement u_status instead of setting them,
   * because we don't want to put ourselves to sleep if we were not sleeping
   * before but just calling this callback to check on our status.
   */

  UAREA.u_status++; // wakeup
  while(1)
  {
    if (ipc_msgring->flag == 0)
    {
      UAREA.u_status--; // back to sleep
      return;
    }
    
    /* must cpulock the release of ring buffer because we may get
     * called again before this try resumes from yield */
    cpulock_acquire();

    msg_type = ((u_int *)(ipc_msgring->space))[0]; 
    caller = ((u_int *)(ipc_msgring->space))[1];

    if (msg_type == IPC_MSG_REQUEST)
    {
      struct ipc_msgring_ent* ringptr = ipc_msgring;
      int r2;
      
      task = ((int *)(ipc_msgring->space))[2];
      arg1 = ((int *)(ipc_msgring->space))[3];
      arg2 = ((int *)(ipc_msgring->space))[4];
      reqid = ((int *)(ipc_msgring->space))[5];

      ipc_msgring = ipc_msgring->next;
  
      cpulock_release();
      r = ipc1_demultiplex(task, arg1, arg2, IPC_MSG, caller, ringptr, &r2);
      ipc_clear_msgent(ringptr);

      if (r < 0)
        ipc_replymsg(caller, -1, errno, reqid);
      else
        ipc_replymsg(caller, r, r2, reqid);
    } 
    
    else if (msg_type == IPC_MSG_REPLY)
    {

      task = ((int *)(ipc_msgring->space))[2];
      arg1 = ((int *)(ipc_msgring->space))[3];
      reqid = ((int *)(ipc_msgring->space))[4];
      
      ipc_clear_msgent(ipc_msgring);
      ipc_msgring = ipc_msgring->next;
      
      cpulock_release();
      ipc2_demultiplex(task, arg1, reqid, IPC_MSG, caller);
    }

    else 
      cpulock_release();
  }
}






/********************************************
 **     Common/Generic IPC definitions     **
 ********************************************/

void
ipc_init(void)
{
  int i;

  for(i=0; i<MAX_OUTSTANDING_IPCS; i++)
    outstanding_ipcs[i].status = IPC_REQ_FREE;

  for(i=0; i<MAX_IPC_HANDLERS; i++)
    ipc_request_handlers[i] = 0L;

  UAREA.u_entipc1 = (u_int) ipc1_entry;
  UAREA.u_entipc2 = (u_int) ipc2_entry;

  ipc1_in = default_ipc1;
  ipc2_in = default_ipc2;

  sys_set_allowipc1(XOK_IPC_ALLOWED);
  sys_set_allowipc2(XOK_IPC_ALLOWED);

  ipc_setup_msgring();
  wk_register_extra (ipc_msgring_pred_constr, ipc_msgring_callback, 0L, 8);
}


void
ipc_child_init(void)
{
  int i;

  /* clear outstanding IPCs, no longer our problem */
  for(i=0; i<MAX_OUTSTANDING_IPCS; i++)
    outstanding_ipcs[i].status = IPC_REQ_FREE;

  ipc_setup_msgring_afterfork();
}


int
ipc_register_handler(u_short id, ipc_request_handler_ptr_t h)
{
  if (id > MAX_IPC_NUMBER || ipc_request_handlers[id] != 0L || h == 0)
    RETERR(V_INVALID);
 
  ipc_request_handlers[id] = h;
  return 0;
}


int
ipc_unregister_handler(u_short id)
{
  if (id <= MAX_IPC_NUMBER)
    ipc_request_handlers[id] = 0;
  return 0;
}

