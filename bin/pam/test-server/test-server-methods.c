#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <xok/sys_ucall.h>
#include <xok/pctr.h>

#include "test-server.h"

arg_t         *s_state;     // the shared state
unsigned int  initialized;

arg_t         private_area; // the private state
arg_t         *p_state = &private_area;



#ifdef IPC

extern int fd[2];

int S_empty (void)
{
  return 0;
}

int S_init (int unused)
{
  unsigned int pte;
  
  if (read (fd[0], &pte, sizeof(pte)) != sizeof(pte)) {
    printf ("S_init: read from pipe failed\n");
    return (-1);
  }

  if (pte == 0)
    return 0;
  s_state = (arg_t *) malloc (sizeof(arg_t));
  if (s_state == NULL  ||  sys_self_insert_pte (0, pte, (uint) s_state) != 0) {
    printf ("SERVER: Error in S_init\n");
    return -1;
  }
  p_state->nbytes=0;
  initialized = 1;
  return 0;
}

int S_push (void)
{
  if (initialized == 0  ||  p_state->nbytes + s_state->nbytes > MAX_LEN) {
    printf ("SERVER: Error in S_push\n");
    return -1;
  }
  memcpy (p_state->buf + p_state->nbytes, s_state->buf, s_state->nbytes);
  p_state->nbytes += s_state->nbytes;
  return 0;
}

int S_pop (void)
{
  if (initialized == 0  ||  p_state->nbytes - s_state->nbytes < 0) {
    printf ("SERVER: Error in S_pop\n");
    return -1;
  }
  memcpy (p_state->buf + p_state->nbytes - 1, s_state->buf, s_state->nbytes);
  p_state->nbytes -= s_state->nbytes;
  return 0;
}

#define DELTA (200 * 1000 * 100)
int S_compute (void)
{
  pctrval t0, t=0;

  for (t0=rdtsc() ; t < t0 + DELTA ; t=rdtsc());
  return 0;
}

int dispatch (int unused0, int meth_num, int optional, int unused, uint caller) 
{
  switch (meth_num) {
  case INIT:  return (S_init (optional));
  case PUSH:  return (S_push ());
  case POP:   return (S_pop ());
  case EMPTY: return (S_empty());
  case COMPUTE: return (S_compute());
  }
  return -1;
}

#else // meaning IPC is turned off

int S_empty (void)
{
  return 0;
}

int S_init (int pte)
{
  if (pte == 0)
    return 0;
  s_state = (arg_t *) malloc (sizeof(arg_t));
  if (s_state == NULL  ||  sys_self_insert_pte (0, pte, (uint) s_state) != 0) {
    printf ("SERVER: Error in S_init\n");
    return -1;
  }
  p_state->nbytes=0;
  initialized = 1;
  return 0;
}

int S_push (void)
{
  if (initialized == 0  ||  p_state->nbytes + s_state->nbytes > MAX_LEN) {
    printf ("SERVER: Error in S_push\n");
    return -1;
  }
  memcpy (p_state->buf + p_state->nbytes, s_state->buf, s_state->nbytes);
  p_state->nbytes += s_state->nbytes;
  return 0;
}

int S_pop (void)
{
  if (initialized == 0  ||  p_state->nbytes - s_state->nbytes < 0) {
    printf ("SERVER: Error in S_pop\n");
    return -1;
  }
  memcpy (p_state->buf + p_state->nbytes - 1, s_state->buf, s_state->nbytes);
  p_state->nbytes -= s_state->nbytes;
  return 0;
}

#define DELTA (200 * 1000 * 100)
int S_compute (void)
{
  pctrval t0, t=0;

  for (t0=rdtsc() ; t < t0 + DELTA ; t=rdtsc());
  return 0;
}

int dispatch (int unused0, int meth_num, int optional, int unused, uint caller) 
{
  switch (meth_num) {
  case INIT:  return (S_init (optional));
  case PUSH:  return (S_push ());
  case POP:   return (S_pop ());
  case EMPTY: return (S_empty());
  case COMPUTE: return (S_compute());
  }
  return -1;
}
#endif  // IPC
