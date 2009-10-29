#include <sys/types.h>
#include <sys/uio.h>
#include <exos/process.h>
#include <stdlib.h>
#include <xok/pctr.h>
#include <xok/sys_ucall.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test-server.h"

arg_t         *shared_state;  // This is where we pass the args
unsigned int  initialized;

uint envid;

void main (int argc, char **argv) 
{
  pid_t    pid;
  uint     pte;
#define SIZE (1024*1024)
  char     buf[SIZE];

  pid = fork();

  //==================== SERVER ====================//
  if (pid == 0) { 
    ipc_register(IPC_USER, dispatch);
    envid = sys_geteid();

    sleep(5);

    return;
  }

  //==================== CLIENT ====================//

  envid = pid2envid(pid);
  yield(envid);

  // The "shared" memory used to communicate with the server

  shared_state = (arg_t *) malloc (sizeof(arg_t));
  if (shared_state == NULL) {
    printf ("[CLIENT] Couldn't allocate shared_state memory\n");
    exit(-1);
  }
  shared_state->nbytes = 12345; // touch the memory
  
  // Figure out the pte corresponding to the "shared" memory

  pte = sys_read_pte ((uint) shared_state, 0, sys_geteid());
  if (pte == 0) {
    printf ("[CLIENT] Found null pte\n");
    exit (-1);
  }

  // Now start the operations

  S_INIT(pte);

  {
    int i, t=0, rounds=atoi(argv[1]);

    for (i=0 ; i < rounds ; i++) {
      pctrval t0 = rdtsc();
      S_EMPTY(0);
      t += (rdtsc() - t0);
      bzero ((void *)buf, SIZE);
    }

    printf ("%d\t%d\n", rounds, (unsigned int) t/200);
  }

  
}

int S_INIT (uint pte)
{
  return (sipcout(envid, IPC_USER, INIT, pte, 0));
}

int S_PUSH (byte *buf, int num_bytes)
{
  if (num_bytes > MAX_LEN)
    return -1;
  shared_state->nbytes = num_bytes;
  memcpy (shared_state->buf, buf, num_bytes);
  return (sipcout(envid, IPC_USER, PUSH, 0, 0));
}

int S_POP (int num_bytes, byte *buf)
{
  if (num_bytes > MAX_LEN)
    return -1;
  shared_state->nbytes = num_bytes;                          
  if (sipcout(envid, IPC_USER, POP, 0, 0) != 0)
    return -1;
  if (shared_state->nbytes != num_bytes)
    return -1;
  memcpy (buf, shared_state->buf, num_bytes);
  return 0;
}

int S_EMPTY (int unused)
{
  if (sipcout(envid, IPC_USER, EMPTY, 0, 0) != 0)
    return -1;
  return 0;
}

int S_COMPUTE (void)
{
  int res;
  if ((res = sipcout(envid, IPC_USER, COMPUTE, 0, 0)) != 0)
    return -1;
  return res;
}
