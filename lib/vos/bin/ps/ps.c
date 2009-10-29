
#include <stdio.h>
#include <string.h>

#include <xok/sys_ucall.h>
#include <xok/env.h>

#include <vos/kprintf.h>
#include <vos/proc.h>


/* ps
 *
 * prints process information. vos ps list kernel Env structures with some
 * extra information obtained from Uenv.
 */


/* returns char representation for e->env_status */
static inline char
e_status(s) 
{
  if (s == ENV_OK) return 'R';
  else if (s == ENV_DEAD) return 'Z';
  return '?';
}

/* returns char representation for e->env_u->u_status */
static inline char
u_status_char(s) 
{
  if (s <= U_SLEEP) return 's';
  else if (s >= U_RUN) return 'r';
  return '?';
}

/* returns char representation for e->env_u->u_status */
char usv[4];
static inline char*
u_status_val(s) 
{
  if (s < 0) 
    sprintf(usv,"%d",s);
  else 
    sprintf(usv," %d",s);
  usv[2]='\0';
  return usv;
}

int main()
{
  int i;
  struct Env* e;

#ifndef __SMP__
  printf("     PID       EID  STAT     TICKS  CXTCNT  COMMAND\n");
#else
  printf("     PID       EID  STAT     TICKS  CXTCNT  CPU  COMMAND\n");
#endif

  for(i=0; i<NENV; i++)
  {
    e = &__envs[i]; 
    if (e->env_status != ENV_FREE)
    {
      struct Uenv uarea;
      char name[18];

      while(sys_rdu(0, e->env_id, &uarea) < 0);

      strcpy(name,uarea.name);
#if 0
      if (strlen(uarea.name) > 17)
      {
	name[7] = '.';
	name[8] = '.';
	name[9] = '.';
	memcpy(&name[10],&uarea.name[strlen(uarea.name)-6],7);
	name[17] = '\0';
      }
#endif

#ifndef __SMP__
      printf("%8d  %8d  %c%c%s  %8d  %6d  %s\n",
	     uarea.pid,   			/* pid */
	     e->env_id,				/* env id */
	     e_status(e->env_status),		/* env status */
             u_status_char(uarea.u_status),	/* uarea status */
             u_status_val(uarea.u_status),	/* uarea status */
	     e->env_ticks,			/* ticks */
	     e->env_ctxcnt,			/* context count */
	     name);				/* name */
#else
      printf("%8d  %8d  %c%c%s  %8d  %6d   %d%c  %s\n",
	     uarea.pid,   			/* pid */
	     e->env_id,				/* env id */
	     e_status(e->env_status),		/* env status */
             u_status_char(uarea.u_status),	/* uarea status */
             u_status_val(uarea.u_status),	/* uarea status */
	     e->env_ticks,			/* ticks */
	     e->env_ctxcnt,			/* context count */
	     e->env_last_cpu,			/* last CPU */
	     (e->env_cur_cpu == -1)?' ':'*',	/* active? */
	     name);				/* name */
#endif
    }
  }

  return 0;
}


