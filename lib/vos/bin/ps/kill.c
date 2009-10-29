#include <signal.h>

#include <xok/ipc.h>
#include <xok/env.h>
#include <xok/sys_ucall.h>

#include <vos/proc.h>
#include <vos/kprintf.h>
#include <vos/ipc.h>
#include <vos/signals.h>


/* kill
 *
 * terminate or signal a process
 */


static int 
parse_cmd(int argc, char *argv[])
{
  int i;
  u_int envid;
  int signal, force;
  extern int atoi(const char*, int);
  
  force = 0;
  signal = 15;

  for(i=1;i<argc;i++)
  {
    if (strncmp(argv[i],"-f",strlen(argv[i]))==0)
      force = 1;
    
    else if (argv[i][0]=='-' && strlen(argv[i])>1 && strlen(argv[i])<=3)
    {
      int s = atoi(&argv[i][1],strlen(argv[i])-1);
      
      if (s >= SIGHUP && s <= SIGUSR2)
	signal = s;
      else
	return -1;
    }
   
    else 
    {
      envid = atoi(argv[i], strlen(argv[i]));
 
      printf("signaling %d\n", envid);
      if (!force) 
	kill(envid, signal);
      else 
	sys_env_free(0,envid);
    }
  }

  return 0;
}


int 
main(int argc, char *argv[])
{
  if (parse_cmd(argc, argv)<0)
  {
    printf("illegal command: %s [-f] [-n] id ...\n",argv[0]);
    return -1;
  }
  return 0;
}


