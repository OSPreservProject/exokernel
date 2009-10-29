#ifndef _DEBUG_H
#define _DEBUG_H

#include <stdio.h>
#include <xok_include/errno.h>

#include <unistd.h>    /* for sleep */
#include <xok/sys_ucall.h>
#include <xok/console.h>
#include "monitor.h"
#include "types.h"

void debugger(void);

void show_cregs(void);
void show_regs(char *, int);
void show_ints(int, int);
void _leaveemu(int, char *, unsigned int);
#define leaveemu(i) _leaveemu(i, __FILE__, __LINE__);

/* 0xd6 looks like an unused opcode */
#define GARBAGE   0xd6
#define GARBAGE_W 0xd6d6d6d6

#define ERR_OK        0
#define ERR           1
#define ERR_MEM       2
#define ERR_PT        3		/* page tables */
#define ERR_DISK      4
#define ERR_CONFIG    5
#define ERR_DESCR     6		/* descriptor related  (idt, gdt, ...) */
#define ERR_IO        7
#define ERR_UNIMPL  254
#define ERR_ASSERT  255

#ifndef NDEBUG
 void _Assert(char *, unsigned int);
#endif

#ifndef NDEBUG
 #define ASSERT(f) \
    if (f) \
        {} \
    else   \
        _Assert(__FILE__, __LINE__)
#else
 #define ASSERT(f)
#endif



#define DTRACE    0x1
#define DDEBUG    0x2
#define DINFO     0x4
#define DWARN     0x8
#define DERROR    0x10
#define DLOG      0x20
#define DDISK     0x40
#define DVIDEO    0x80
#define DKYBD     0x100
#define DIO       0x200
#define DCONFIG   0x400
#define DMEM      0x800
#define DCMOS     0x1000

extern FILE *dbg_fd;
extern int debug_flags;

#ifndef NDEBUG
#define debugprintf(type,fmt,a...) \
{ \
  if ((debug_flags & (type)) == (type)) {\
    if (dbg_fd) \
	fprintf(dbg_fd, fmt,##a);\
    else \
        printf(fmt,##a);\
  } \
}
#else
#define debugprintf(type,fmt,a...)
#endif

#define always_printf(fmt,a...) \
  if (dbg_fd) \
    fprintf(dbg_fd, fmt,##a);\
  printf(fmt,##a);


#define DEBUG_ON()   debug_flags = 0xffffffff;
#define DEBUG_OFF()  debug_flags = 0;

#define trace(fmt,a...)      debugprintf(DTRACE        ,fmt,##a)
#define debug(fmt,a...)      debugprintf(DDEBUG        ,fmt,##a)
#define info(fmt,a...)       debugprintf(DINFO         ,fmt,##a)
#define warn(fmt,a...)       debugprintf(DWARN         ,fmt,##a)
#define error(fmt,a...)      debugprintf(DERROR        ,fmt,##a)

#define dprintf(fmt,a...)    always_printf(             fmt,##a)

#define d_printf(fmt,a...)   debugprintf(DDISK         ,fmt,##a)
#define trace_disk(fmt,a...) debugprintf(DDISK |DTRACE ,fmt,##a)
#define debug_disk(fmt,a...) debugprintf(DDISK |DDEBUG ,fmt,##a)
#define error_disk(fmt,a...) debugprintf(DDISK |DERROR ,fmt,##a)

#define v_printf(fmt,a...)   debugprintf(DVIDEO        ,fmt,##a)
#define trace_vid(fmt,a...)  debugprintf(DVIDEO|DTRACE ,fmt,##a)
#define debug_vid(fmt,a...)  debugprintf(DVIDEO|DDEBUG ,fmt,##a)
#define error_vid(fmt,a...)  debugprintf(DVIDEO|DERROR ,fmt,##a)

#define k_printf(fmt,a...)   debugprintf(DKYBD         ,fmt,##a)

#define i_printf(fmt,a...)   debugprintf(DIO           ,fmt,##a)
#define trace_io(fmt,a...)   debugprintf(DIO   |DTRACE ,fmt,##a)
#define debug_io(fmt,a...)   debugprintf(DIO   |DDEBUG ,fmt,##a)

#define c_printf(fmt,a...)   debugprintf(DCONFIG       ,fmt,##a)

#define trace_mem(fmt,a...)  debugprintf(DMEM  |DTRACE ,fmt,##a)
#define debug_mem(fmt,a...)  debugprintf(DMEM  |DTRACE ,fmt,##a)
#define error_mem(fmt,a...)  debugprintf(DMEM  |DERROR ,fmt,##a)

#define trace_cmos(fmt,a...) debugprintf(DCMOS |DTRACE ,fmt,##a)
#define debug_cmos(fmt,a...) debugprintf(DCMOS |DTRACE ,fmt,##a)
#define error_cmos(fmt,a...) debugprintf(DCMOS |DERROR ,fmt,##a)

void push_debug_flags(void);
void pop_debug_flags(void);

#endif
