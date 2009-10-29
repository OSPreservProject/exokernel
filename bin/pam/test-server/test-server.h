#ifndef __TEST_SERVER__
#define __TEST_SERVER__

#include <exos/ipc.h>
#include <xok/types.h>
#include <xuser.h>

#define MAX_LEN 3000

typedef unsigned char byte;

typedef struct arg {
  uint  nbytes;
  byte  buf[MAX_LEN];
} arg_t;

#define INIT 0
#define PUSH 1
#define POP  2
#define EMPTY 3
#define COMPUTE 4

int dispatch (int, int, int, int, uint);

int S_INIT (uint);
int S_PUSH (byte *, int);
int S_POP (int, byte *);
int S_EMPTY (int);
int S_COMPUTE (void);


#endif // __TEST_SERVER__
