#include <exos/ipc.h>
#include <exos/process.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <xok/pctr.h>
#include <xok/sys_ucall.h>
#include <xok/types.h>
#include <xuser.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LEN 3000

#define INIT    ((unsigned char) 0)
#define PUSH    ((unsigned char) 1)
#define POP     ((unsigned char) 2)
#define EMPTY   ((unsigned char) 3)

/* The file descriptors for the ends of the pipes */
static int pfd1[2], pfd2[2];
#define CLI_IN  (pfd1[0])
#define CLI_OUT (pfd2[1])
#define SRV_IN  (pfd2[0])
#define SRV_OUT (pfd1[1])

/*---------------------------------------------------------------------------
 * The SERVER's state
 *---------------------------------------------------------------------------*/

unsigned char Buf[MAX_LEN];
unsigned int  NBytes;

/*===========================================================================
 * The SERVER's functions
 *===========================================================================*/

int srv_empty (void)
{
  return 0;
}

/*------------------------------------------------------------
 * srv_init initializes the abstraction. 
 * actual ARGS: none
 *------------------------------------------------------------*/

int srv_init (void)
{
  NBytes = 0;
  return 0;
}

/*------------------------------------------------------------
 * srv_push pushes bytes from the pipe onto the pseudo-stack.
 * actual ARGS: number of bytes
 *              data to be pushed
 *------------------------------------------------------------*/

int srv_push (void)
{
  int nbytes;

  if (read(SRV_IN, &nbytes, sizeof(nbytes)) != sizeof(nbytes)) {
    perror ("srv_push (reading nbytes)");
    return (-1);
  }
  if (NBytes + nbytes > MAX_LEN) {
    printf ("srv_push: Too many bytes to push.\n");
    return (-1);
  }
  if (read(SRV_IN, &Buf[NBytes], nbytes) != nbytes) {
    perror ("srv_push (reading data)");
    return (-1);
  }
  NBytes += nbytes;
  return 0;
}

/*------------------------------------------------------------
 * srv_pop pops bytes off the pseudo-stack and writes them to 
 * the pipe.
 * actual ARGS: number of bytes
 *------------------------------------------------------------*/

int srv_pop (void)
{
  int nbytes;

  if (read(SRV_IN, &nbytes, sizeof(nbytes)) != sizeof(nbytes)) {
    perror ("srv_pop (reading nbytes)");
    return (-1);
  }
  if (NBytes - nbytes < 0) {
    printf ("srv_pop: Too many bytes to pop.\n");
    return (-1);
  }
  if (write(SRV_OUT, &Buf[NBytes-nbytes], nbytes) != nbytes) {
    printf ("srv_pop (writing data): ERROR");
    return (-1);
  }
  NBytes -= nbytes;
  return 0;
}

/*===========================================================================
 * The SERVER dispatcher. This function waits for commands to come
 * down the pipe and then dispatches them. It also sends down the pipe
 * the result of the operation.
 *===========================================================================*/

int dispatch (void) 
{
  unsigned char command, result;

  while (1) {
    if (read(SRV_IN, &command, sizeof(command)) != sizeof(command)) {
      perror ("dispatch (reading command)");
      return (-1);
    }
    switch (command) {
    case INIT:  result = srv_init(); break;
    case PUSH:  result = srv_push(); break;
    case POP:   result = srv_pop();  break;
    case EMPTY: result = srv_empty(); break;
    default:    printf ("dispatch: unknown command\n"); result = -1;
    }
    if (write(SRV_OUT, &result, sizeof(result)) != sizeof(result)) {
      perror ("dispatch (writing result)");
      return (-1);
    }
  }
}

/*===========================================================================
 * The client's functions.
 *===========================================================================*/

#define SEND_COMMAND(command)                              \
do {                                                       \
  unsigned char val = command;                             \
  if (write (CLI_OUT, &val, sizeof(val)) != sizeof(val)) { \
    perror("Requesting " # command);                       \
    return (-1);                                           \
  }                                                        \
} while (0)

#define SEND_VAR(var,func)                                 \
do {                                                       \
  if (write (CLI_OUT, &var, sizeof(var)) != sizeof(var)) { \
    perror (#func ## "(sending var)");                     \
    return (-1);                                           \
  }                                                        \
} while (0)

#define SEND_DATA(buf,nbytes,func)                         \
do {                                                       \
  if (write (CLI_OUT, buf, nbytes) != nbytes) {            \
    perror (#func ## "(sending data)");                    \
    return (-1);                                           \
  }                                                        \
} while (0)

#define CHECK(command)                                     \
do {                                                       \
  unsigned char val;                                       \
  if (read (CLI_IN, &val, sizeof(val)) != sizeof(val)) {   \
    perror("Completing " # command);                       \
    return (-1);                                           \
  }                                                        \
  return 0;                                                \
} while (0)
  

int cli_empty (void)
{
  SEND_COMMAND(EMPTY);
  CHECK(EMPTY);
}

/*------------------------------------------------------------
 * Initialize the abstraction.
 *------------------------------------------------------------*/

int cli_init (void)
{
  SEND_COMMAND(INIT);
  CHECK(INIT);
}

/*------------------------------------------------------------
 * Sends data to be pushed. Number of bytes followed by the
 * data itself.
 *------------------------------------------------------------*/

int cli_push (unsigned char *buf, int nbytes)
{
  SEND_COMMAND(PUSH);
  SEND_VAR(nbytes,cli_push);
  SEND_DATA(buf,nbytes,cli_push);
  CHECK(PUSH);
}

/*------------------------------------------------------------
 * Requests data to be popped. Sends the number of bytes and
 * then reads from the pipe the data into 'buf'.
 *------------------------------------------------------------*/

int cli_pop (unsigned char *buf, int nbytes)
{
  SEND_COMMAND(POP);
  SEND_VAR(nbytes,cli_pop);
  if (read (CLI_IN, buf, nbytes) != nbytes) {
    perror ("cli_pop (reading data)");
    return (-1);
  }
  CHECK(POP);
}

/*=========================================================================*/

void main (int argc, char **argv) 
{
  pid_t    pid;
  char     buf[1024*1024];

  if (pipe(pfd1) < 0  ||  pipe(pfd2) < 0) {
    perror ("test-server.c: (creating pipes)");
    exit (1);
  }

  pid = fork();

  //==================== SERVER ====================//
  if (pid == 0) { 
    dispatch();
    printf ("SERVER: dispatch returned\n");
    return;
  }

  //==================== CLIENT ====================//

  cli_init();
  {
    int i, t=0, rounds=atoi(argv[1]);

    for (i=0 ; i < rounds ; i++) {
      pctrval t0 = rdtsc();
      cli_empty();
      t += (rdtsc() - t0);
      bzero ((void *)buf, 1024*1024);
    }

    printf ("IPC : time = %8u usec\n", (unsigned int) t/200);
  }
}
