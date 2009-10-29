#ifndef __WAFER_H__
#define __WAFER_H__

#include <xok/env.h>

/* From entry.S */
extern void xue_epilogue (void);
extern void xue_prologue (void);
extern void xue_fault (void);
extern void xue_yield (void);
extern char edata;
extern char end;


/* from xrt0.c */
extern u_int __envid;
extern struct Env * __curenv;
extern u_int __brkpt;
extern u_int __stkbot;
extern u_int __xstktop;


void __die (void) __attribute__ ((noreturn));

static inline void yield (int envid) {
  UAREA.u_donate = envid;
  asm ("pushl %0\n"
       "\tpushfl\n"
       "\tjmp _xue_yield\n"
       :: "g" (&&next));
  goto next; /* get around warning about unused label */
next:
}

#endif

