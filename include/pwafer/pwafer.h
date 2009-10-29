#ifndef __PWAFER_H__
#define __PWAFER_H__

#include <xok/env.h>

extern void xue_epilogue (void);
extern void xue_prologue (void);
extern void xue_fault (void);
extern void xue_yield (void);
extern char edata;
extern char end;
extern u_int __brkpt;

void __die (void) __attribute__ ((noreturn));

static inline void yield (int envid) 
{
  UAREA.u_donate = envid;
  asm ("pushl %0\n"
       "\tpushfl\n"
       "\tjmp _xue_yield\n"
       :: "g" (&&next));
  goto next; /* get around warning about unused label */
next:
}

extern int getpid();

#define CAP_USER 0


#include <pwafer/locks.h>

/* thread control block */
typedef struct ThreadCtrlBlk
{
  u_int t_id;
  u_int t_status;
#define T_FREE		0
#define T_READY		1
#define T_RUN		2
#define T_SUSPEND	3
#define T_TRQ_UP	4
  u_int t_eip;
  struct ThreadCtrlBlk *t_next;

#if 0
  u_int t_edi;
  u_int t_esi;
  u_int t_eax;
  u_int t_ebx;
  u_int t_ecx;
  u_int t_edx;
  u_int t_ebp;
  u_int t_esp;
#endif

} TCrBlk;


/* extended process structure */
typedef struct ExtProc
{
  u_int envid;
  struct Env* curenv;
  u_int stacktop;
  u_int stackbot;
  u_int stackbotlim;
  u_int xstktop;
  u_int unused;
  struct ThreadCtrlBlk *tcb;

  char  xstk[1024];
} ExtProc;


/* FIFO run queue per CPU */
typedef struct TRunQueue
{
  TCrBlk head;		/* 16 bytes */
  TCrBlk *tail;		/*  4 bytes */
  u_int size;		/*  4 bytes */
  spinlock_t trq_lock;	/*  8 bytes */	/* not used */
} TRQ;


#define MAX_NUM_THREADS 1024
#define KT_STACKSIZE 	128*1024*1024

extern void pw_init();


#define __eproc 	(UAREA.ext_proc)
#define __tcb   	(__eproc->tcb)
#define __envid 	(__eproc->envid)
#define __curenv 	(__eproc->curenv)
#define __threadid 	(__tcb->t_id)

#define __stktop 	(__eproc->stacktop)
#define __stkbot 	(__eproc->stackbot)
#define __stkbotlim 	(__eproc->stackbotlim)
#define __xstktop 	(__eproc->xstktop)


#endif

