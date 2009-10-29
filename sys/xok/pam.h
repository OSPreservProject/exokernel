#ifndef __PROTMETH_H__
#define __PROTMETH_H__

#include <xok/mmu.h>

// Protmeths are always mapped between PROT_AREA_BOT and
// PROT_AREA_TOP. PROT_AREA_BOT is placed half-way between 0x0 and
// SHARED_LIBRARY_START. This gives us 128 MBytes for protmeths and
// leaves about 96 MBytes for the process.

#define PROT_AREA_BOT  0x8000000
#define PROT_AREA_TOP 0x10000000

#define CAP_PROT      10 /* slot where the protmeth capability lives */
#define MAX_NMETHODS   5 /* max. number of methods allowed in an abstraction */
#define MAX_SIZE      (PROT_AREA_TOP - PROT_AREA_BOT)
#define MAX_PAGES     (MAX_SIZE / NBPG)

// The fault handlers structure is used to pass to the protected
// method the entry points of the user-vectored fault/interrupt
// handlers that were installed prior to its invocation.

typedef struct _metadata {
  uint entepilogue;  // Epilogue entry point
  uint entprologue;  // Prologue entry point
  uint entfault;     // Page fault entry
  uint entipc1;      // An IPC entry point
  uint entipc2;      // A second IPC entry point
  uint entipca;      // ASH IPC
  uint entrtc;       // Run-time clock handler
} metadata_t;

// The method table structure is used to specify abstractions. Some of
// the fields are only used by the kernel.

typedef struct _mtab {
  uint          nmethods;            // the number of methods in this abstraction
  uint          start[MAX_NMETHODS]; // the methods' user-space start VAs
#ifdef KERNEL
  cap           state_cap;           // capability needed to access the state
  uint          npages;              // number of pages in this abstraction
  Pte           ptes[MAX_PAGES];     // pte's for phys. pages containing the meths
  Pte           pte;                 // metadata pte
  uint          va;                  // VA where to map metadata pte
  metadata_t    handlers;            // the abstraction's fault handlers
#endif
} mtab_t;

// Status codes returned by protmeth syscalls.

#define PROT_OK        0  /* everything fine */
#define PROT_NABS     -1  /* max. number of abstractions already defined */
#define PROT_NO_CAP   -2  /* couldn't find valid cap in specified slot */
#define PROT_TOO_BIG  -3  /* maximum size of abstraction exceeded */
#define PROT_BAD_VA   -4  /* specified virtual address is invalid */
#define PROT_NMETHS   -5  /* too many methods in abstraction */
#define PROT_LOW      -6  /* start address for abstraction is too low */
#define PROT_HIGH     -7  /* end of abstraction is too high */
#define PROT_NO_PAGE  -8  /* couldn't allocate a metadata page for abstraction */
#define PROT_NO_ABS   -9  /* requested abstraction does not exists */
#define PROT_NO_METH -10  /* requested method does not exist in requested abs */
#define PROT_NO_MAP  -11  /* could not map metadata page for requested abs */

// Helpers

#define ensure(cond,errcode)  do {         \
  if (! (cond))                            \
    return (errcode);                      \
} while (0)                                

#define perr(errcode)  do { \
  printf ("%s (%d): ", __FILE__, __LINE__); \
  switch (errcode) { \
  case PROT_OK: printf ("Everything OK\n"); break; \
  case PROT_NABS: printf ("Max. number of abstractions already defined\n"); break; \
  case PROT_NO_CAP: printf ("Couldn't find valid cap in specified slot\n"); break; \
  case PROT_TOO_BIG: printf ("Maximum size of abstraction exceeded\n"); break; \
  case PROT_BAD_VA: printf ("Specified virtual address is invalid\n"); break; \
  case PROT_NMETHS: printf ("Too many methods in abstraction\n"); break; \
  case PROT_LOW: printf ("Start address for abstraction is too low\n"); break; \
  case PROT_HIGH: printf ("End of abstraction is too high\n"); break; \
  case PROT_NO_PAGE: printf ("Couldn't allocate a metadata page for abstraction\n"); break; \
  case PROT_NO_ABS: printf ("Requested abstraction is not defined\n"); break; \
  case PROT_NO_METH: printf ("Requested method does not exist in requested abs\n"); break; \
  case PROT_NO_MAP: printf ("Couldn't map required metadata page\n"); break; \
  default: printf ("Unknown error %d\n", errcode); \
  } \
} while (0)

#endif // __PROTMETH_H__
