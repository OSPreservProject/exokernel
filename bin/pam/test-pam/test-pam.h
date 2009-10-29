#include <xok/env.h>
#include <stdio.h>
#include <xok/mmu.h>
#include <xok/sys_ucall.h>
#include <xok/pam.h>

#define SHARED_STATE ((uint) 0xC000000)

#define CAP_PROT 10
#define CAP_ROOT 11
  
typedef struct _c {
  uint		 nbytes;
  unsigned char  *buf;
} arg_t;

#define PTR(p)	  ((uint) p)

void S_init_stub (void);
void S_push_stub (void);
void S_pop_stub (void);
void S_empty_stub (void);
int  x_handler (u_int, u_int, u_int, u_int, u_int);
void local_memcpy (void *, void *, uint);

#define MAX_LEN 3000  /* max. # of bytes we can store in state */

/* The IDs for the different protected methods */

#define INIT	0
#define PUSH	1
#define POP	2
#define EMPTY	3
#define COMPUTE 4

inline int sys_prot_call(uint AbsID,	 /* abstraction ID		  */
			 uint MethodID)  /* method ID abstraction's state */
{
    printf ("[sys_prot_call] ARGUMENTS:\n");
    printf ("	AbsID = %d\n", (int) AbsID);
    printf ("	MethodID = %d\n", (int) MethodID);
    printf ("	0x%x 0x%x\n", (uint) &AbsID, (uint) &MethodID);
    return 0;
}


typedef unsigned char byte;

#define S_INIT(unused)				     \
do {						     \
  int res;					     \
  if ((res =sys_prot_call (INIT | 0x80000000, 0))) { \
     perr (res);				     \
     return;					     \
   }						     \
} while (0)		   


#define S_PUSH(buf, num_bytes)			     \
do {						     \
  int res;					     \
  args.nbytes = num_bytes;			     \
  args.buf = buf;				     \
  if ((res = sys_prot_call (PUSH, (uint)&args))) {     \
     perr (res);				     \
     return;					     \
  }						     \
} while (0)


#define S_POP(num_bytes, buf)			     \
do {						     \
  int res;					     \
  args.nbytes = num_bytes;			     \
  args.buf = buf;				     \
  if ((res = sys_prot_call (POP, (uint)&args))) {     \
     perr (res);				     \
     return;					     \
  }						     \
} while (0)


#define S_EMPTY(unused)				     \
do {						     \
  int res;					     \
  if ((res = sys_prot_call (EMPTY, (uint)&args)))  {	     \
    perr (res);					     \
    return;					     \
  }						     \
} while (0)


#define S_COMPUTE				     \
do {						     \
  int res;					     \
  if ((res = sys_prot_call (COMPUTE,(uint) &args))) {	   \
    perr (res);					     \
    return;					     \
  }						     \
} while (0)
