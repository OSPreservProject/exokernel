#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <sys/wait.h>

#ifdef EXOPC
#include <xok/env.h>
#endif

void usage(void);
extern char **environ;
int fork_execvp(const char *path, char *const argv[]);
void start_child(void);
void wait_child(void);
int dotest(int memory);

#define NRMEMORY 1 * 1024 /* 4 * 1024 * 1024 */
#define NRJOBS 1
#define NRITER 500

#define USEFORK			/* if we want to fork or exec the child */

int dotest_flag = 0;
int memory_flag = NRMEMORY;
int jobs_flag = NRJOBS;
int iter_flag = NRITER;
char *progname;
char **argvcp;

int
main (int argc, char **argv)
{
  int i;

  argvcp = argv;
  progname = *argv++;			/* skip progname */
  if (!strcmp(progname,"dotest")) {
    dotest_flag = 1;
  } 
  while(*argv != (char *)0) {
    if (!strcmp(*argv,"--help")) {
      usage();
    } else
    if (!strcmp(*argv,"-m")) {
      argv++;
      if (*argv) {
	memory_flag = atoi(*argv);
	memory_flag *= 1024;
	if (memory_flag < 0) {
	  usage();
	}
      } else {
	usage();
      }
    } else
    if (!strcmp(*argv,"-j")) {
      argv++;
      if (*argv) {
	jobs_flag = atoi(*argv);
	if (jobs_flag <= 0) {
	  usage();
	}
#ifdef EXOPC
	if (jobs_flag >= U_MAXCHILDREN) {
	  printf ("Max %d children at once\n", U_MAXCHILDREN);
	  jobs_flag = U_MAXCHILDREN;
	}
#endif
      } else {
	usage();
      }
    } else
    if (!strcmp(*argv,"-i")) {
      argv++;
      if (*argv) {
	iter_flag = atoi(*argv);
	if (iter_flag <= 0) {
	  usage();
	}
      } else {
	usage();
      }
    } else { usage();}
    
    argc--;
    argv++;
  }
  printf("memory: %d, jobs: %d. iter: %d dotest: %d\n",
	 memory_flag,jobs_flag,iter_flag, dotest_flag);
  if (dotest_flag) {
    exit (dotest(memory_flag));
  }

  *argvcp = "dotest";

  /* start jobs_flags processes */
  for (i = 0; i < jobs_flag; i++) {
    start_child();
  }
  /* as each starts launch another one */
  for (i = 0; i < iter_flag; i++) {
    wait_child();
    start_child();
  }
  /* wait for the last jobs_flags processes */
  for (i = 0; i < jobs_flag; i++) {
    wait_child();
  }
  printf("Ok\n");
  return 0;
}

void usage(void) {
  extern char *__progname;
  printf("Usage: %s [--help] [-m #] [-j #]",__progname);
  printf("--help: you get this message\n");
  printf("-m amount of memory to malloc and test in kilobytes\n");
  printf("-j number of simulaneous processes banging on the memory\n");
  printf("-i number of iterations\n");
  printf("with no flags it starts testing memory\n");
  printf("default: amount of memory: %d, jobs: %d, iterations: %d\n",NRMEMORY,NRJOBS,NRITER); 
  exit(0);
}

int dotest(int memory) {
  unsigned int *p,*p0;
  int i;
  if (memory == 0)
    return 0;
  p = p0 = (int *)malloc(memory);
  /*  printf("child %d malloced %d bytes\n",getpid(),memory);*/
  if (!p) {
    printf("could not malloc %d\n",memory);
    return -1;
  }

  for (i = 0; i < memory/sizeof(int) ; i++) {
    *p = i;
    /*printf("%p *p: %d\n",p,*p);*/
    p++;
  }
  for (i = 0; i < memory/sizeof(int) ; i++) {
    /*printf("%p *p0: %d\n",p,*p0);*/
    if (*p0++ != i) {
      p0--;
      printf("memory differs at %p (from beginning %d) value: %d, expected %d\n",
	     p0,i,*p0,i);
      return -2;
    }
  }

  return 0;

}

void start_child(void) {
  int pid;
  /*  printf("starting %s\n",progname); */
#ifdef USEFORK
    if ((pid = fork()) == 0) {
      exit(dotest(memory_flag));
    }
    if (pid < 0) {
      printf("problems forking: %d\n",pid);
      printf("FAILED\n");
      exit(-1);
    }
#else 
    if ((pid = fork_execvp(progname,argvcp)) < 0) {
      printf("problems starting %s, returned: %d\n",progname,pid);
      printf("FAILED\n");
      exit(-1);
    }
#endif
}


void wait_child(void) {
  int status;
  /*  printf("waiting for child\n"); */
  wait(&status);
  if (WIFEXITED (status) && WEXITSTATUS(status) == 0)
    return;
  printf("FAILED child returned %d\n",status);
  exit(-1);
  /*  printf("child returned: %d\n",status); */
}
