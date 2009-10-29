//
// This is a temporary include file. The stuff in here should go in xok/pam.h
// between #ifdefs eventually.
//


#include <xok/env.h>
#include <xok/pmap.h>
#include <xok/capability.h>
#include <xok/pam.h>

#ifndef __MY_PROTMETH_H__
#define __MY_PROTMETH_H__

#define require(cond,msg...)  do {         \
  if (! (cond)) {                          \
    printf ("[protmeth.c %d] ", __LINE__); \
    printf (msg);                          \
    return (-1);                           \
  }                                        \
} while (0)


// Max. number of simultaneous abstractions
#define MAX_NABS      5
// Max. number of methods in an abstraction

// Extract the method id from a uint
#define METH_ID(id)   (id & 0xFFFF)
// Extract the abstraction id from a uint
#define ABS_ID(id)    ((id >> 16) & 0x7FFF)
// Extract the code mapping bit from a uint
#define MAP_BIT(id)   (id >> 31)

typedef unsigned char uchar;

typedef enum {ACL, ENV} caplist_t;

void pp_dump (struct Ppage *, char *);
void cap_dump (cap *, caplist_t);
void env_caps_dump (struct Env *, char *);
void dump_mtab (mtab_t *, char *);
void dump_abs_tab (char *);
void dump_pte (uint, char *);
void dump_trap (void);
void dump_ppn (uint);

#define EXTRA_DEBUG 1

#if EXTRA_DEBUG
#define DBG(msg...)  do { printf ("[protmeth.c %d] ", __LINE__); printf(msg); } while(0)
#else
#define DBG(msg...)  { }
#endif

#define KERN(va) (ptov (va2pa ((void *) (va))))

#define SN (SYS_self_insert_pte_range)

#define RESET_TSC \
do { \
  __asm __volatile  \
    ("\n" \
     "movl $16, %%ecx\n" \
     "xorl %%eax, %%eax\n" \
     "wrmsr\n" : \
     : : \
     "eax", "ecx"); \
} while (0)
  


#endif
