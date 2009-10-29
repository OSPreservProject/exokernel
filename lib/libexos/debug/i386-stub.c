
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

/****************************************************************************rem

		THIS SOFTWARE IS NOT COPYRIGHTED

   HP offers the following for use in the public domain.  HP makes no
   warranty with regard to the software or it's performance and the
   user accepts the software "AS IS" with all faults.

   HP DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD
   TO THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES
   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

****************************************************************************/

/****************************************************************************
 *  Header: remcom.c,v 1.34 91/03/09 12:29:49 glenne Exp $
 *
 *  Module name: remcom.c $
 *  Revision: 1.34 $
 *  Date: 91/03/09 12:29:49 $
 *  Contributor:     Lake Stevens Instrument Division$
 *
 *  Description:     low level support for gdb debugger. $
 *
 *  Considerations:  only works on target hardware $
 *
 *  Written by:      Glenn Engel $
 *  ModuleState:     Experimental $
 *
 *  NOTES:           See Below $
 *
 *  Modified for 386 by Jim Kingdon, Cygnus Support.
 *
 *  To enable debugger support, two things need to happen.  One, a
 *  call to set_debug_traps() is necessary in order to allow any breakpoints
 *  or error conditions to be properly intercepted and reported to gdb.
 *  Two, a breakpoint needs to be generated to begin communication.  This
 *  is most easily accomplished by a call to breakpoint().  Breakpoint()
 *  simulates a breakpoint by executing a trap #1.
 *
 *  The external function exceptionHandler() is
 *  used to attach a specific handler to a specific 386 vector number.
 *  It should use the same privilege level it runs at.  It should
 *  install it as an interrupt gate so that interrupts are masked
 *  while the handler runs.
 *  Also, need to assign exceptionHook and oldExceptionHook.
 *
 *  Because gdb will sometimes write to the stack area to execute function
 *  calls, this program cannot rely on using the supervisor stack so it
 *  uses it's own stack area reserved in the int array remcomStack.
 *
 *************
 *
 *    The following gdb commands are supported:
 *
 * command          function                               Return value
 *
 *    g             return the value of the CPU registers  hex data or ENN
 *    G             set the value of the CPU registers     OK or ENN
 *
 *    mAA..AA,LLLL  Read LLLL bytes at address AA..AA      hex data or ENN
 *    MAA..AA,LLLL: Write LLLL bytes at address AA.AA      OK or ENN
 *
 *    c             Resume at current address              SNN   ( signal NN)
 *    cAA..AA       Continue at address AA..AA             SNN
 *
 *    s             Step one instruction                   SNN
 *    sAA..AA       Step one instruction from AA..AA       SNN
 *
 *    k             kill
 *
 *    ?             What was the last sigval ?             SNN   (signal NN)
 *
 * All commands and responses are sent with a packet which includes a
 * checksum.  A packet consists of
 *
 * $<packet info>#<checksum>.
 *
 * where
 * <packet info> :: <characters representing the command or response>
 * <checksum>    :: < two hex digits computed as modulo 256 sum of <packetinfo>>
 *
 * When a packet is received, it is first acknowledged with either '+' or '-'.
 * '+' indicates a successful transfer.  '-' indicates a failed transfer.
 *
 * Example:
 *
 * Host:                  Reply:
 * $m0,10#2a               +$00010203040506070809101112131415#42
 *
 ****************************************************************************/

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <exos/uidt.h>
#include <exos/conf.h>
#include <exos/process.h>
#include <exos/cap.h>
#include <exos/vm.h>
#include <exos/vm-layout.h>
#include <xok/mmu.h>
#include <xok/pmap.h>
#include <exos/osdecl.h>
#include <string.h>
#include <stdio.h>
#include <exos/critical.h>
#include <exos/fpu.h>

#include <xok/env.h>
#include <xok/env.h>
#include <xok/mmu.h>
#include <xok/sys_ucall.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <signal.h>
#include <assert.h>

#include <sys/wait.h>
#include <exos/process.h>

#define STUB_DEBUG 0
#define SHOW_STACK 0
#define DEBUG_PORT_MIN  9999
#define DEBUG_PORT_MAX  9999


int    __in_debugger = 0; /* used in page_fault_handler */

static void set_debug_traps();
static u_int  gdb_save_ip = 0;
static u_char gdb_save_instr = 0; 
static int didinit = 0; 
static int sockfd = 0;
static int gdbsockfd = 0;
static struct sockaddr_in addr;

/*
 * pending signals are deliverd by the prologue
 *
 * on entry to the prologue our stack looks like this:
 * ---------------------------------------------------
 *
 *  fpu state [optional]  <----- our_env.env_tf.tf_esp
 *  di
 *  si 
 *  ebp
 *  esp
 *  ebx
 *  edx
 *  ecx
 *  eax
 *  eflags
 *  eip                  <------ this is what we want
 */

static unsigned int get_eip (void)
{
  unsigned int esp = __curenv->env_tf.tf_esp;

  /* skip past fpu state, if we saved it */
  if (UAREA.u_fpu)
    esp += FPU_SAVE_SZ; 

  /* skip down to eip */
  esp += 36;

  return (*(u_int *)esp);
}



extern void show_stack(void);
asm(".text");
asm ("SSS:");
asm (".ascii \" %x \\0\"");
asm(".globl _show_stack");
asm("_show_stack:");

asm("pushl 0(%esp)");
asm("pushl $SSS");
asm("call _kprintf");
asm("addl $8,%esp");

asm("pushl 4(%esp)");
asm("pushl $SSS");
asm("call _kprintf");
asm("addl $8,%esp");

asm("pushl 8(%esp)");
asm("pushl $SSS");
asm("call _kprintf");
asm("addl $8,%esp");

asm("pushl 12(%esp)");
asm("pushl $SSS");
asm("call _kprintf");
asm("addl $8,%esp");

asm("pushl 16(%esp)");
asm("pushl $SSS");
asm("call _kprintf");
asm("addl $8,%esp");

asm("pushl 20(%esp)");
asm("pushl $SSS");
asm("call _kprintf");
asm("addl $8,%esp");

asm("pushl 24(%esp)");
asm("pushl $SSS");
asm("call _kprintf");
asm("addl $8,%esp");

asm("pushl 24(%esp)");
asm("pushl $SSS");
asm("call _kprintf");
asm("addl $8,%esp");

asm("ret");

void debug_shutdown(void) {
  if (sockfd)
    close(sockfd);
  if (gdbsockfd)
    close(gdbsockfd);
}

static int getDebugChar(void) 
{
  char c;
  if (read(gdbsockfd, &c, 1) < 0) 
    {
      /*       kprintf(("read error\n"); */
      debug_shutdown();
      exit(-1);
    }
  return(c);
}

static int putDebugChar(int c) 
{
  if (write(gdbsockfd, &c, 1) < 0)
    {
      /*       kprintf(("write error\n"); */
      debug_shutdown();
      exit(-1);
    }

  return 1;
}


static void exceptionHandler(int exception_num, void *exception_addr)
{
  uidt_register_sfunc(exception_num, (u_int)exception_addr);
  //sys_set_idt_handler(exception_num, (u_int)exception_addr, 1);
}



/* *********************************************************************
 * Initializes the debugging support.                            
 * This is a two step process.  
 *
 * The first step is to handle the communication to gdb. This involves
 * creating a (tcp) socket on port "port" and waiting until gdb
 * connects.
 * 
 * The second step is to transfer control to gdb for the first time. 
 * This is done via the "int 3". 
 * *********************************************************************/

static int debug_startup_internal(int port, int do_int)
{
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    return 0;

  bzero((char *) &addr, sizeof(addr));
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port        = htons(port);
  
  if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) 
    {
      close(sockfd);
      return 0;
    }

  if (listen(sockfd, 1) < 0) 
    {
      close(sockfd);
      return 0;
    }

  kprintf("(%s envid:%d pid:%d) waiting for gdb to connect on port %d...\n", UAREA.name, __envid, getpid(), port);
  if ((gdbsockfd = accept(sockfd, (struct sockaddr *)NULL, (int *)NULL)) < 0) 
    {
      kprintf("debug:accept error");
      close(sockfd);
      return 0;
    }

  set_debug_traps();
  didinit = 1; 

  if (do_int)
    asm("   int $3");

  return 1;
}

/*
 * sets up the port and does an "int 3"
 */
int debug_startup(int port)
{
  return debug_startup_internal (port, 1);
}




/************************************************************************
 *
 * external low-level support routines
 */
typedef void (*ExceptionHook)(int);   /* pointer to function with int parm */
typedef void (*Function)();           /* pointer to a function */

/* extern Function exceptionHandler(); */ /* assign an exception handler */
/* extern ExceptionHook exceptionHook; */ /* hook variable for errors/exceptions */

/************************************************************************/
/* BUFMAX defines the maximum number of characters in inbound/outbound buffers*/
/* at least NUMREGBYTES*2 are needed for register packets */
#define BUFMAX 400

static char initialized;  /* boolean flag. != 0 means we've been initialized */

int     _exos_remote_debug = 0;
/*  debug >  0 prints ill-formed commands in valid packets & checksum errors */

static void waitabit();

static const char hexchars[]="0123456789abcdef";

/* Number of bytes of registers.  */
#define NUMREGBYTES 64
enum regnames {EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI,
	       PC /* also known as eip */,
	       PS /* also known as eflags */,
	       CS, SS, DS, ES, FS, GS};

/*
 * these should not be static cuz they can be used outside this module
 */
int registers[NUMREGBYTES/4];

#define GDBSTACKSIZE 10000
int remcomStack[GDBSTACKSIZE/sizeof(int)];
static int* stackPtr = &remcomStack[GDBSTACKSIZE/sizeof(int) - 1];

extern void
return_to_prog ();

/* Restore the program's registers (including the stack pointer, which
   means we get the right stack and don't have to worry about popping our
   return address and any stack frames and so on) and return.  */
asm(".text");
asm(".globl _return_to_prog");
asm("_return_to_prog:");
asm("        movw _registers+44, %ss");
asm("        movl _registers+16, %esp");
asm("        movl _registers+4, %ecx");
asm("        movl _registers+8, %edx");
asm("        movl _registers+12, %ebx");
asm("        movl _registers+20, %ebp");
asm("        movl _registers+24, %esi");
asm("        movl _registers+28, %edi");
asm("        movw _registers+48, %ds");
asm("        movw _registers+52, %es");
asm("        movw _registers+56, %fs");
asm("        movw _registers+60, %gs");
asm("        movl _registers+36, %eax");
asm("        pushl %eax");  /* saved eflags */
asm("        movl _registers+40, %eax");
asm("        pushl %eax");  /* saved cs */
asm("        movl _registers+32, %eax");
asm("        pushl %eax");  /* saved eip */
asm("        movl _registers, %eax");
/* use iret to restore pc and flags together so
   that trace flag works right.  */
asm("        iret");

#define BREAKPOINT() asm("   int $3");

/* Put the error code here just in case the user cares.  */
int gdb_i386errcode;
/* Likewise, the vector number here (since GDB only gets the signal
   number through the usual means, and that's not very specific).  */
/* int gdb_i386vector = -1; */

/* GDB stores segment registers in 32-bit words (that's just the way
   m-i386v.h is written).  So zero the appropriate areas in registers.  */
#define SAVE_REGISTERS1() \
  asm ("movl %eax, _registers");                                   	  \
  asm ("movl %ecx, _registers+4");			  		     \
  asm ("movl %edx, _registers+8");			  		     \
  asm ("movl %ebx, _registers+12");			  		     \
  asm ("movl %ebp, _registers+20");			  		     \
  asm ("movl %esi, _registers+24");			  		     \
  asm ("movl %edi, _registers+28");			  		     \
  asm ("movw $0, %ax");							     \
  asm ("movw %ds, _registers+48");			  		     \
  asm ("movw %ax, _registers+50");					     \
  asm ("movw %es, _registers+52");			  		     \
  asm ("movw %ax, _registers+54");					     \
  asm ("movw %fs, _registers+56");			  		     \
  asm ("movw %ax, _registers+58");					     \
  asm ("movw %gs, _registers+60");			  		     \
  asm ("movw %ax, _registers+62");
#define SAVE_ERRCODE() \
  asm ("popl %ebx");                                  \
  asm ("movl %ebx, _gdb_i386errcode");
#define SAVE_REGISTERS2() \
  asm ("popl %ebx"); /* old eip */			  		     \
  asm ("movl %ebx, _registers+32");			  		     \
  asm ("popl %ebx");	 /* old cs */			  		     \
  asm ("movl %ebx, _registers+40");			  		     \
  asm ("movw %ax, _registers+42");                                           \
  asm ("popl %ebx");	 /* old eflags */		  		     \
  asm ("movl %ebx, _registers+36");			 		     \
  /* Now that we've done the pops, we can save the stack pointer.");  */   \
  asm ("movw %ss, _registers+44");					     \
  asm ("movw %ax, _registers+46");     	       	       	       	       	     \
  asm ("movl %esp, _registers+16");

/* See if mem_fault_routine is set, if so just IRET to that address.  */
#define CHECK_FAULT() \
  asm ("cmpl $0, _mem_fault_routine");					   \
  asm ("jne mem_fault");

asm (".text");
asm ("mem_fault:");
/* OK to clobber temp registers; we're just going to end up in set_mem_err.  */
/* Pop error code from the stack and save it.  */
asm ("     popl %eax");
asm ("     movl %eax, _gdb_i386errcode");

asm ("     popl %eax"); /* eip */
/* We don't want to return there, we want to return to the function
   pointed to by mem_fault_routine instead.  */
asm ("     movl _mem_fault_routine, %eax");
asm ("     popl %ecx"); /* cs (low 16 bits; junk in hi 16 bits).  */
asm ("     popl %edx"); /* eflags */

/* Remove this stack frame; when we do the iret, we will be going to
   the start of a function, so we want the stack to look just like it
   would after a "call" instruction.  */
asm ("     leave");

/* Push the stuff that iret wants.  */
asm ("     pushl %edx"); /* eflags */
asm ("     pushl %ecx"); /* cs */
asm ("     pushl %eax"); /* eip */

/* Zero mem_fault_routine.  */
asm ("     movl $0, %eax");
asm ("     movl %eax, _mem_fault_routine");

asm ("iret");

#define CALL_HOOK() asm("call _remcomHandler");

/* This function is called when a i386 exception occurs.  It saves
 * all the cpu regs in the _registers array, munges the stack a bit,
 * and invokes an exception handler (remcom_handler).
 *
 * stack on entry:                       stack on exit:
 *   old eflags                          vector number
 *   old cs (zero-filled to 32 bits)
 *   old eip
 *
 */
extern void _catchException3();
asm(".text");
asm(".globl __catchException3");
asm("__catchException3:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushl $3");
CALL_HOOK();

/* Same thing for exception 1.  */
extern void _catchException1();
asm(".text");
asm(".globl __catchException1");
asm("__catchException1:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushl $1");
CALL_HOOK();

/* Same thing for exception 0.  */
extern void _catchException0();
asm(".text");
asm(".globl __catchException0");
asm("__catchException0:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushl $0");
CALL_HOOK();

/* Same thing for exception 4.  */
extern void _catchException4();
asm(".text");
asm(".globl __catchException4");
asm("__catchException4:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushl $4");
CALL_HOOK();

/* Same thing for exception 5.  */
extern void _catchException5();
asm(".text");
asm(".globl __catchException5");
asm("__catchException5:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushl $5");
CALL_HOOK();

/* Same thing for exception 6.  */
extern void _catchException6();
asm(".text");
asm(".globl __catchException6");
asm("__catchException6:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushl $6");
CALL_HOOK();

/* Same thing for exception 7.  */
extern void _catchException7();
asm(".text");
asm(".globl __catchException7");
asm("__catchException7:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushl $7");
CALL_HOOK();

/* Same thing for exception 8.  */
extern void _catchException8();
asm(".text");
asm(".globl __catchException8");
asm("__catchException8:");
SAVE_REGISTERS1();
SAVE_ERRCODE();
SAVE_REGISTERS2();
asm ("pushl $8");
CALL_HOOK();

/* Same thing for exception 9.  */
extern void _catchException9();
asm(".text");
asm(".globl __catchException9");
asm("__catchException9:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushl $9");
CALL_HOOK();

/* Same thing for exception 10.  */
extern void _catchException10();
asm(".text");
asm(".globl __catchException10");
asm("__catchException10:");
SAVE_REGISTERS1();
SAVE_ERRCODE();
SAVE_REGISTERS2();
asm ("pushl $10");
CALL_HOOK();

/* Same thing for exception 12.  */
extern void _catchException12();
asm(".text");
asm(".globl __catchException12");
asm("__catchException12:");
SAVE_REGISTERS1();
SAVE_ERRCODE();
SAVE_REGISTERS2();
asm ("pushl $12");
CALL_HOOK();

/* Same thing for exception 16.  */
extern void _catchException16();
asm(".text");
asm(".globl __catchException16");
asm("__catchException16:");
SAVE_REGISTERS1();
SAVE_REGISTERS2();
asm ("pushl $16");
CALL_HOOK();

/* For 13, 11, and 14 we have to deal with the CHECK_FAULT stuff.  */

/* Same thing for exception 13.  */
extern void _catchException13 ();
asm (".text");
asm (".globl __catchException13");
asm ("__catchException13:");
CHECK_FAULT();
SAVE_REGISTERS1();
SAVE_ERRCODE();
SAVE_REGISTERS2();
asm ("pushl $13");
CALL_HOOK();

/* Same thing for exception 11.  */
extern void _catchException11 ();
asm (".text");
asm (".globl __catchException11");
asm ("__catchException11:");
CHECK_FAULT();
SAVE_REGISTERS1();
SAVE_ERRCODE();
SAVE_REGISTERS2();
asm ("pushl $11");
CALL_HOOK();



extern void _catchException14 ();
/* Same thing for exception 14.  */
asm (".text");
asm (".globl __catchException14");
asm ("__catchException14:");
CHECK_FAULT();
SAVE_REGISTERS1();
SAVE_ERRCODE();
SAVE_REGISTERS2();
asm ("pushl $14");
CALL_HOOK();


/* stack on entry (grows down):
 * trap eflags 
 * trap eip     <--- %esp
 ****
 * we transform this to look like an 
 * exception stack
 *   trap eflags
 *   trap %cs       -- use the current cs, b/c it is the same as the trap time cs 
 *   trap %eip  
 *   trap err code -- basically unused so just put 0 here
 */

asm (".text");
asm (".globl __DebugPfaultEntry");
asm ("__DebugPfaultEntry:");
asm ("pushl (%esp)");
asm ("movw %cs, 4(%esp)");
asm ("movl $0, 6(%esp)");
asm ("pushl $0");
asm ("jmp __catchException14");
/*
 * remcomHandler is a front end for handle_exception.  It moves the
 * stack pointer into an area reserved for debugger use.
 */
asm("_remcomHandler:");
asm("           popl %eax");        /* pop off return address     */
asm("           popl %eax");      /* get the exception number   */
asm("		movl _stackPtr, %esp"); /* move to remcom stack area  */
asm("		pushl %eax");	/* push exception onto stack  */
asm("		call  _handle_exception");    /* this never returns */

static void _returnFromException()
{
  return_to_prog ();
}

static int hex(ch)
char ch;
{
  if ((ch >= 'a') && (ch <= 'f')) return (ch-'a'+10);
  if ((ch >= '0') && (ch <= '9')) return (ch-'0');
  if ((ch >= 'A') && (ch <= 'F')) return (ch-'A'+10);
  return (-1);
}


/* scan for the sequence $<data>#<checksum>     */
static void getpacket(buffer)
char * buffer;
{
  unsigned char checksum;
  unsigned char xmitcsum;
  int  i;
  int  count;
  char ch;

  do {
    /* wait around for the start character, ignore all other characters */
    while ((ch = (getDebugChar() & 0x7f)) != '$');
    checksum = 0;
    xmitcsum = -1;

    count = 0;

    /* now, read until a # or end of buffer is found */
    while (count < BUFMAX) {
      ch = getDebugChar() & 0x7f;
      if (ch == '#') break;
      checksum = checksum + ch;
      buffer[count] = ch;
      count = count + 1;
      }
    buffer[count] = 0;

    if (ch == '#') {
      xmitcsum = hex(getDebugChar() & 0x7f) << 4;
      xmitcsum += hex(getDebugChar() & 0x7f);
      if ((_exos_remote_debug ) && (checksum != xmitcsum)) {
        fprintf (stderr ,"bad checksum.  My count = 0x%x, sent=0x%x. buf=%s\n",
		 checksum,xmitcsum,buffer);
      }

      if (checksum != xmitcsum) putDebugChar('-');  /* failed checksum */
      else {
	 putDebugChar('+');  /* successful transfer */
	 /* if a sequence char is present, reply the sequence ID */
	 if (buffer[2] == ':') {
	    putDebugChar( buffer[0] );
	    putDebugChar( buffer[1] );
	    /* remove sequence chars from buffer */
	    count = strlen(buffer);
	    for (i=3; i <= count; i++) buffer[i-3] = buffer[i];
	 }
      }
    }
  } while (checksum != xmitcsum);

#if STUB_DEBUG
  printf("DBG received:%s\n", buffer);
  fflush(stdout);
#endif
}

/* send the packet in buffer.  */


static void putpacket(buffer)
char * buffer;
{
  unsigned char checksum;
  int  count;
  char ch;

  /*  $<packet info>#<checksum>. */
  do {
    putDebugChar('$');
    checksum = 0;
    count    = 0;
    
    while (ch=buffer[count]) {
      if (! putDebugChar(ch)) return;
      checksum += ch;
      count += 1;
    }
    
    putDebugChar('#');
    putDebugChar(hexchars[checksum >> 4]);
    putDebugChar(hexchars[checksum % 16]);
    
  } while ((getDebugChar() & 0x7f) != '+');
  
#if STUB_DEBUG
  printf("DBG sent:%s\n", buffer);
  fflush(stdout);
#endif

}

static char  remcomInBuffer[BUFMAX];
static char  remcomOutBuffer[BUFMAX];
static short error;


static void debug_error(format, parm)
char * format;
char * parm;
{
  if (_exos_remote_debug) fprintf (stderr,format,parm);
}

/* Address of a routine to RTE to if we get a memory fault.  */
static void (*volatile mem_fault_routine)() = NULL;

/* Indicate to caller of mem2hex or hex2mem that there has been an
   error.  */
static volatile int mem_err = 0;

static void
set_mem_err ()
{
#if SHOW_STACK
  printf("<set_mem_err>\n");
  kprintf("<set_mem_err>\n");
  kprintf("[SME: ");
  printf("[SME: ");
  show_stack();
  kprintf("SME]\n");
  printf("SME]\n");
#endif

  mem_err = 1;
}

/* These are separate functions so that they are so short and sweet
   that the compiler won't save any registers (if there is a fault
   to mem_fault, they won't get restored, so there better not be any
   saved).  */


static unsigned char get_char (char *addr)
{
#if SHOW_STACK
  kprintf("[SS: ");
  printf("[SS: ");
  show_stack();
  kprintf("SS]\n");
  printf("SS]\n");
#endif

  return *(unsigned char *)addr;
}


static void
set_char (char *addr, char val)
{
  int orig = vpt[PGNO((u_int)addr)]; 

  if (!(orig & PG_W)) 
    assert (sys_self_mod_pte_range(0, PG_COW, PG_RO, (u_int)addr, 1) >= 0);

  *(unsigned char *)addr = val;

  /*   if (!(orig & PG_W)) */
  /*     assert (sys_self_mod_pte_range(0, 0, PG_W, (u_int)addr, 1) >= 0); */
}


/* convert the memory pointed to by mem into hex, placing result in buf */
/* return a pointer to the last char put in buf (null) */
/* If MAY_FAULT is non-zero, then we should set mem_err in response to
   a fault; if zero treat a fault like any other fault in the stub.  */
static char* mem2hex(mem, buf, count, may_fault)
char* mem;
char* buf;
int   count;
int may_fault;
{
      int i;
      unsigned char ch;

#if STUB_DEBUG
      printf("mem2hex: mem=%x buf=%x count=%d may_fault=%d...\n", (u_int)mem, (u_int)buf, count, may_fault);
      kprintf("mem2hex: mem=%x buf=%x count=%d may_fault=%d...\n", (u_int)mem, (u_int)buf, count, may_fault);
#endif

      if (may_fault)
	  mem_fault_routine = set_mem_err;
      for (i=0;i<count;i++) {
#if STUB_DEBUG
	  printf("mem2hex:getting_char i=%d mem=%x\n", i, (u_int)mem);
	  kprintf("mem2hex:getting_char i=%d mem=%x\n", i, (u_int)mem);
#endif
          ch = get_char (mem++);
#if STUB_DEBUG
	  printf("mem2hex:got char\n");
	  kprintf("mem2hex:got char\n");
#endif
	  if (may_fault && mem_err)
	    {
#if STUB_DEBUG
	      printf("mem2hex:did fault\n");
	      kprintf("mem2hex:did fault\n");
#endif
	      return (buf);
	    }
          *buf++ = hexchars[ch >> 4];
          *buf++ = hexchars[ch % 16];
      }
      *buf = 0;
      if (may_fault)
	  mem_fault_routine = NULL;
      return(buf);
}

/* convert the hex array pointed to by buf into binary to be placed in mem */
/* return a pointer to the character AFTER the last byte written */
static char* hex2mem(buf, mem, count, may_fault)
char* buf;
char* mem;
int   count;
int may_fault;
{
      int i;
      unsigned char ch;

#if STUB_DEBUG
      printf("hex2mem: mem=%x buf=%x count=%d may_fault=%d...\n", (u_int)mem, (u_int)buf, count, may_fault);
      kprintf("hex2mem: mem=%x buf=%x count=%d may_fault=%d...\n", (u_int)mem, (u_int)buf, count, may_fault);
#endif
      if (may_fault)
	  mem_fault_routine = set_mem_err;
      for (i=0;i<count;i++) {
          ch = hex(*buf++) << 4;
          ch = ch + hex(*buf++);

#if STUB_DEBUG
	  printf("setting addr=%04x to=%02x...", (u_int)mem, (unsigned char)ch);   fflush(stdout); 
#endif
          set_char (mem++, ch);
#if STUB_DEBUG
	   	  printf("done!\n");  	  fflush(stdout); 
#endif

	  if (may_fault && mem_err)
	    return (mem);
      }
      if (may_fault)
	  mem_fault_routine = NULL;
      return(mem);
}

/* this function takes the 386 exception vector and attempts to
   translate this number into a unix compatible signal value */
static int computeSignal( exceptionVector )
int exceptionVector;
{
  int sigval;
  switch (exceptionVector) {
    case 0 : sigval = 8; break; /* divide by zero */
    case 1 : sigval = 5; break; /* debug exception */
    case 3 : sigval = 5; break; /* breakpoint */
    case 4 : sigval = 16; break; /* into instruction (overflow) */
    case 5 : sigval = 16; break; /* bound instruction */
    case 6 : sigval = 4; break; /* Invalid opcode */
    case 7 : sigval = 8; break; /* coprocessor not available */
    case 8 : sigval = 7; break; /* double fault */
    case 9 : sigval = 11; break; /* coprocessor segment overrun */
    case 10 : sigval = 11; break; /* Invalid TSS */
    case 11 : sigval = 11; break; /* Segment not present */
    case 12 : sigval = 11; break; /* stack exception */
    case 13 : sigval = 11; break; /* general protection */
    case 14 : sigval = 11; break; /* page fault */
    case 16 : sigval = 7; break; /* coprocessor error */
    default:
      sigval = 7;         /* "software generated"*/
  }
  return (sigval);
}

/**********************************************/
/* WHILE WE FIND NICE HEX CHARS, BUILD AN INT */
/* RETURN NUMBER OF CHARS PROCESSED           */
/**********************************************/
static int hexToInt(char **ptr, int *intValue)
{
    int numChars = 0;
    int hexValue;

    *intValue = 0;

    while (**ptr)
    {
        hexValue = hex(**ptr);
        if (hexValue >=0)
        {
            *intValue = (*intValue <<4) | hexValue;
            numChars ++;
        }
        else
            break;

        (*ptr)++;
    }

    return (numChars);
}



/*
 * This is the signal handler registered for signal 30 (SIGUSR1).
 * This function replaces the instruction, that this currently executing program
 * will return to (after it delivers the sig 30, with a breakpoint instruction.
 * Saving the instruction, actually the 1st byte of it, in gdb_save_intr, and 
 * the ip of that instruction in gdb_save_ip.
 */
void debug_signal_handler (int sig)
{
  int ip = get_eip();
  
  kprintf("(%s envid:%d pid:%d) In debug_signal_handler pc=0x%x.\n", 
	  UAREA.name, __envid, getpid(), ip);

  assert(gdb_save_ip == 0);
  assert(gdb_save_instr == 0);
  set_debug_traps();

  gdb_save_ip = ip;
  gdb_save_instr = *(unsigned char *)gdb_save_ip;
  /*   printf ("foo ppc=0x%x instr=0x%x\n", UAREA.u_ppc, gdb_save_instr); */
#define INT3_OPCODE 0xcc
  set_char ((char *)gdb_save_ip, INT3_OPCODE);
  UAREA.u_status = U_RUN;
}



/*
 * This function does all command procesing for interfacing to gdb.
 */
static int in_exception_handler = 0;
static void handle_exception(int exceptionVector)
{
  int    sigval;
  int    addr, length;
  char * ptr;
  int    newPC;

  __in_debugger = 1;

  if (in_exception_handler)
    {
      kprintf("(%s envid:%d pid:%d pc=0x%x vct=%d) Recursive debugger exception detected.\n", 
	      UAREA.name, __envid, getpid(), registers[PC], exceptionVector);
      assert(0);
    }

  in_exception_handler++;

  if (gdb_save_ip == (registers[PC] - 1))
    {
      registers[PC] -= 1;
      set_char((char *)gdb_save_ip, gdb_save_instr);  
      gdb_save_ip = gdb_save_instr = 0;
    }

  if (!didinit)
    {
      int i, started;
      kprintf("(%s envid:%d pid:%d pc=0x%x cs=0x%x vct=%d errorcode=%d) Unexpected exception. Remote debugging starting.\n", 
	      UAREA.name, __envid, getpid(), registers[PC],
	      registers[CS], exceptionVector, gdb_i386errcode);
      started = 0;
      for (i = DEBUG_PORT_MIN; i <= DEBUG_PORT_MAX; i++)
	{
	  /* usleep(10000000); */
	  if (debug_startup_internal(i,0))
	    {
	      started = 1;
	      break;
	    }
	}

      if (!started)
	{
	  kprintf("Failed to initialize debugging support. Tried ports [%d,%d].\n", DEBUG_PORT_MIN, DEBUG_PORT_MAX);
	  ProcessEnd (W_EXITCODE (0, 0), 0);
	}
    }
    

#if STUB_DEBUG
  printf("entering handle_exception vector=%d...\n", exceptionVector);
#endif


/*   gdb_i386vector = exceptionVector; */

  if (_exos_remote_debug) printf("vector=%d, sr=0x%x, pc=0x%x\n",
			    exceptionVector,
			    registers[ PS ],
			    registers[ PC ]);

  /* reply to host that an exception has occurred */
  sigval = computeSignal( exceptionVector );
  remcomOutBuffer[0] = 'S';
  remcomOutBuffer[1] =  hexchars[sigval >> 4];
  remcomOutBuffer[2] =  hexchars[sigval % 16];
  remcomOutBuffer[3] = 0;

  putpacket(remcomOutBuffer);

  while (1==1) {

    error = 0;
    remcomOutBuffer[0] = 0;
    getpacket(remcomInBuffer);
    switch (remcomInBuffer[0]) {
      case '?' :   remcomOutBuffer[0] = 'S';
                   remcomOutBuffer[1] =  hexchars[sigval >> 4];
                   remcomOutBuffer[2] =  hexchars[sigval % 16];
                   remcomOutBuffer[3] = 0;
                 break;
      case 'd' : _exos_remote_debug = !(_exos_remote_debug);  /* toggle debug flag */
                 break;
      case 'g' : /* return the value of the CPU registers */
                mem2hex((char*) registers, remcomOutBuffer, NUMREGBYTES, 0);
                break;
      case 'G' : /* set the value of the CPU registers - return OK */
                hex2mem(&remcomInBuffer[1], (char*) registers, NUMREGBYTES, 0);
                strcpy(remcomOutBuffer,"OK");
                break;

      /* mAA..AA,LLLL  Read LLLL bytes at address AA..AA */
      case 'm' :
		    /* TRY TO READ %x,%x.  IF SUCCEED, SET PTR = 0 */
                    ptr = &remcomInBuffer[1];
                    if (hexToInt(&ptr,&addr))
                        if (*(ptr++) == ',')
                            if (hexToInt(&ptr,&length))
                            {
                                ptr = 0;
				mem_err = 0;
                                mem2hex((char*) addr, remcomOutBuffer, length, 1);
#if STUB_DEBUG
				printf("handle_exception m(read) mem_err=%d\n", mem_err);
#endif

				if (mem_err) {
				    strcpy (remcomOutBuffer, "E03");
				    debug_error ("memory fault");
				}
                            }

                    if (ptr)
                    {
		      strcpy(remcomOutBuffer,"E01");
		      debug_error("malformed read memory command: %s",remcomInBuffer);
		    }
	          break;

      /* MAA..AA,LLLL: Write LLLL bytes at address AA.AA return OK */
      case 'M' :
		    /* TRY TO READ '%x,%x:'.  IF SUCCEED, SET PTR = 0 */
                    ptr = &remcomInBuffer[1];
                    if (hexToInt(&ptr,&addr))
                        if (*(ptr++) == ',')
                            if (hexToInt(&ptr,&length))
                                if (*(ptr++) == ':')
                                {
				    mem_err = 0;
                                    hex2mem(ptr, (char*) addr, length, 1);

				    if (mem_err) {
					strcpy (remcomOutBuffer, "E03");
					debug_error ("memory fault");
				    } else {
				        strcpy(remcomOutBuffer,"OK");
				    }

                                    ptr = 0;
                                }
                    if (ptr)
                    {
		      strcpy(remcomOutBuffer,"E02");
		      debug_error("malformed write memory command: %s",remcomInBuffer);
		    }
                break;

     /* cAA..AA    Continue at address AA..AA(optional) */
     /* sAA..AA   Step one instruction from AA..AA(optional) */
     case 'c' :
     case 's' :
          /* try to read optional parameter, pc unchanged if no parm */
         ptr = &remcomInBuffer[1];
         if (hexToInt(&ptr,&addr))
             registers[ PC ] = addr;

          newPC = registers[ PC];

          /* clear the trace bit */
          registers[ PS ] &= 0xfffffeff;

          /* set the trace bit if we're stepping */
          if (remcomInBuffer[0] == 's') 
	    {
#if STUB_DEBUG
	      printf("we are **STEPPING**!\n");
	      fflush(stdout);
#endif

	      registers[ PS ] |= 0x100;
	    }
          /*
           * If we found a match for the PC AND we are not returning
           * as a result of a breakpoint (33),
           * trace exception (9), nmi (31), jmp to
           * the old exception handler as if this code never ran.
           */
#if 0
	  /* Don't really think we need this, except maybe for protection
	     exceptions.  */
                  /*
                   * invoke the previous handler.
                   */
                  if (oldExceptionHook)
                      (*oldExceptionHook) (frame->exceptionVector);
                  newPC = registers[ PC ];    /* pc may have changed  */
#endif /* 0 */

	  in_exception_handler--;
	  _returnFromException(); /* this is a jump */

          break;

      /* kill the program */
      case 'k' :  
	debug_shutdown();
	exit(-1); 
#if 0
	/* Huh? This doesn't look like "nothing".
	   m68k-stub.c and sparc-stub.c don't have it.  */
		BREAKPOINT();
#endif
                break;
      } /* switch */

    /* reply to the request */
    putpacket(remcomOutBuffer);
    }
}




/* this function is used to set up exception handlers for tracing and
   breakpoints */
static void set_debug_traps(void)
{
  extern void remcomHandler();
  
  stackPtr  = &remcomStack[GDBSTACKSIZE/sizeof(int) - 1];

  exceptionHandler (0, _catchException0); /* divide by zero */
  exceptionHandler (1, _catchException1); /* debug */

  exceptionHandler (3, _catchException3); /* breakpoint */
  exceptionHandler (4, _catchException4); /* overflow */
  exceptionHandler (5, _catchException5); /* bounds check */
  exceptionHandler (6, _catchException6); /* illegal instruction */
  //  exceptionHandler (7, _catchException7); /* device error */
  /* 7 already handled by fpu code */

  exceptionHandler (10, _catchException10); /* Badd TSS */
  exceptionHandler (11, _catchException11); /* Seg fault */
  exceptionHandler (12, _catchException12); /* stack fault */
  exceptionHandler (13, _catchException13); /* GPF */
  exceptionHandler (14, _catchException14); /* page fault */

  exceptionHandler (16, _catchException16); /* floating point */
  /*   if (exceptionHook != remcomHandler) */
  /*     { */
  /*       oldExceptionHook = exceptionHook; */
  /*       exceptionHook    = remcomHandler; */
  /*     } */
  
  /* In case GDB is started before us, ack any packets (presumably
     "$?#xx") sitting there.  */

  assert(signal(SIGUSR1, debug_signal_handler) >= 0);

  /*   if (gdbsockfd) */
  /*     putDebugChar ('+');  */
  
  initialized = 1;
#if STUB_DEBUG
  kprintf("set_debug_traps exiting...\n");
#endif
}



void remote_debug_initialize(void)
{
  set_debug_traps();
}



/* This function will generate a breakpoint exception.  It is used at the
   beginning of a program to sync up with a debugger and can be used
   otherwise as a quick means to stop program execution and "break" into
   the debugger. */

static void breakpoint()
{
  if (initialized)
#if 0
    handle_exception(3);
#else
    BREAKPOINT();
#endif
  waitabit();
}

int waitlimit = 10000;

#if 0
static void
bogon()
{
  waitabit();
}
#endif

static void
waitabit()
{
  int i;
  for (i = 0; i < waitlimit; i++) ;
}
