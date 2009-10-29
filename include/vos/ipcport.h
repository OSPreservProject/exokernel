/*
 * Copyright MIT 1999
 */

#ifndef __VOS_IPCPORT_H__
#define __VOS_IPCPORT_H__


/* each process keeps a table of IPC ports - shared memory based ipc */
typedef struct
{
  Pte pte;
  u_int va;
  int k;
} ipcport_t;

extern ipcport_t ipcports[NPROC];


/*
 * ipcport_create: creates an ipc communication port (shared memory) with
 * another process, using capability k. Returns 0 if successful. Returns -1
 * otherwise. Errno is set to:
 *  
 *   V_NOMEM:       cannot create the shared memory to use as ports.
 *   V_CANTMAP:     cannot map the shared memory for use as ports.
 *   V_WOULDBLOCK:  pct to the target process timedout.
 *   V_CONNREFUSED: pct to the target process failed in kernel.
 *
 * The va where the port is mapped on can be found in the ipcports directory.
 */
extern int
ipcport_create(int k, pid_t pid);


/*
 * ipcport_name: create the name of the file one should use with open to
 * access the ipc port. Always return 0. Does not check if the pid is valid or
 * not.
 */
extern int
ipcport_name(pid_t pid, char *s);


#endif /* __VOS_IPCPORT_H__ */

