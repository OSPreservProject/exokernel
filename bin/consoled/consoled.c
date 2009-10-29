#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <assert.h>
#include <memory.h>
#include <xok/sysinfo.h>
#include <sys/termios.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#include <sys/utsname.h>	/* for uname */

#include <setjmp.h>
sigjmp_buf env;


/* The /dev/console device does not support any of the terminal semantics,
 therefore, we use this program, consoled, to pass information via a 
 pseudo-terminal back and forth into the console.  It uses two programs.
 consoled m->c which reads data from the master side of the pty and sends it to the
 consoled; and consoled c->m, which reads the raw keyboard and sends it to the master.
 The login program is spawn with stdin, stdout, and stderr bound to the slave side
 of the consoled. /dev/console uses the pc3 terminal interface
 */

#define LOGIN "/usr/bin/login"
#define SZ 1024
#define MASTERFD 14
#define CONSOLEFD 15

/* Internal setproctitle that does not append to current name */
extern void setproctitle0(char *);
void settitle(char *);
void print_banner(void);

int verbose = 0;
int verbosesignalhandlers = 0;

int do_console(int master, int console);
int openpty(int *amaster, int *aslave, char *name, 
	struct termios *termp, struct winsize *winp);
char *getloginname(void);
void prstatus(pid_t pid, int status);

/* kill and reap */
void killprog(void);
void killhelper(void);
/* returns process id of new process */
pid_t start_login(int slavefd);

int restartprog = 1;
int console,master,slave;
pid_t helperpid = 0, progpid = 0;

/* signal handlers */
void 
pdone(int signo) {
  if (verbosesignalhandlers) kprintf("pdone signo: %d pid %d\n",signo, getpid());
  if (verbose) printf("pid %d exiting\n",getpid());
  close(master);
  close(console);
  _exit(0);
}

void 
wrapup(int signo) {
  char *msg = "\n\rconsoled is being killed\n\r";
  settitle("consoled: wrapping up");
  if (verbosesignalhandlers) kprintf("wrapup signo: %d pid %d\n",signo, getpid());

  if (getpid() == progpid) exit(0);
  write(console,msg,strlen(msg));
  settitle("consoled: killing helper");
  killhelper();
  settitle("consoled: killing prog");
  killprog();
  if (verbose) printf("exiting %d\n",getpid());
  settitle("consoled: exiting");
  exit(0);
}

void 
pchild(int signo) {
  int status;
  int pid;
  if (verbosesignalhandlers) kprintf("pchild signo %d pid %d\n",signo,getpid());
  if (verbose) printf("%d waiting for child...", getpid());
  pid = wait(&status);

  if (pid == -1) return;
  if (pid == helperpid || pid == progpid) {
    int what = 0;		/* flush all */
    ioctl(master,TIOCFLUSH,&what);
    close(master);
    ioctl(console,TIOCFLUSH,&what);
    close(console);
    
    if (pid == helperpid) 
      killprog();
    else 
      killhelper();

    if (restartprog) { longjmp(env,0);}
  } else {
    fprintf(stderr,"ERROR: I don't have other children, pid %d unknown to me\n",pid);
  }
  exit(0);
}

/* helpers */
void 
prstatus(pid_t pid, int status) {
  if (!verbose) return;
  if (WIFEXITED(status)) 
    printf ("  pid %d exited with %d\n", pid,WEXITSTATUS(status));
  else if (WIFSIGNALED(status))
    printf ("  pid %d signaled by %d\n", pid,WTERMSIG(status));
  else if (WIFSTOPPED(status))
    printf ("  pid %d stopped by %d\n", pid,WSTOPSIG(status));
  else 
    printf ("  BAD WAIT STATUS pid %d, status: 0x%08x errno: %d\n", pid,status,errno);
}

void 
killprog(void) {
  int status;
  kill (progpid, SIGTERM);
  sleep(1);
  kill (progpid, SIGHUP);
  sleep(1);
  kill (progpid, SIGKILL);
  if (waitpid(progpid,&status,(uint)NULL)== progpid) {
    if (verbose) printf("done killing program\n");
    prstatus(progpid, status);
  }
  progpid = 0;
}
void 
killhelper(void) {
  int status;
  if (kill (helperpid, SIGUSR2) != -1) {
    if (waitpid(helperpid,&status,(uint)NULL) == helperpid) {
      if (verbose) printf("done killing helper\n");
      prstatus(helperpid, status);
    } else {
      fprintf(stderr,"error reaping helper %d\n",helperpid);
    }
  }
  helperpid = 0;
}

void 
print_banner(void) {
  struct utsname uts;
  if (uname(&uts) != 0) {
    printf("Welcome\n\n");
  } else {
    char *name;
    name = ttyname(0);
    if (name == NULL) name = "*";
    else
      if (rindex(name,'/')) {name = rindex(name,'/');name++;}
    printf("\n%s/%s (%s) (%s)\n\n",
	   uts.sysname, uts.machine, uts.nodename, name);
  }
}

void
settitle(char *name) {
  setproctitle0(name);
}




/* MAIN */
int 
main(int argc, char **argv) {
  pid_t pid;
  struct sigaction sa;

  if (argc < 1) {
    fprintf(stderr,"Usage: echocon program args...\n");
    exit(-1);
  }
  if (geteuid() != 0) {
    fprintf(stderr,"You must be root in order to run %s\n",argv[0]);
    exit(-1);
  }

  if (fork()) exit(0);		/* decouple from terminal */
  pid = setsid();
  if (pid == -1) {
    fprintf(stderr,"setsid failed: %d,%d\n",pid,errno);
    exit(-1);
  }

  setjmp(env);
  settitle("consoled");
  
  if ((console = open("/dev/console",O_RDWR,0)) < 0) {
    fprintf(stderr,"could not open /dev/console\n");
    exit(-1);
  }

  if (openpty(&master,&slave,0,0,0) < 0) {
    fprintf(stderr,"out of ptys\n");
    return -1;
  }

  /* child signal handler */
  sa.sa_handler = pchild;
  sa.sa_flags = SA_NOCLDSTOP;
  sa.sa_mask = 0xffffffff;
  if (sigaction(SIGCHLD,&sa,NULL) != 0) {
    fprintf(stderr,"sigaction child failed\n");
    exit(-1);
  }
  /* kill consoled, kills it children and exits. */
  sa.sa_handler = wrapup;
  sa.sa_flags = 0;
  sa.sa_mask = 0xffffffff;
  if (sigaction(SIGHUP,&sa,NULL) != 0) {
    fprintf(stderr,"sigaction child failed\n");
    exit(-1);
  }
  sa.sa_handler = wrapup;
  sa.sa_flags = 0;
  sa.sa_mask = 0xffffffff;
#if 0
  if (sigaction(SIGTERM,&sa,NULL) != 0) {
    fprintf(stderr,"sigaction child failed\n");
    exit(-1);
  }
#endif
  progpid = start_login(slave);
  if (progpid == -1) {
    fprintf(stderr,"start login failed\n");
    exit(-1);
  }
  close(slave);
  do_console(master,console);
  /* Never reached */
  assert(0);
  return 0;
}

/* Start helper daemon, and enter into helping mode ourselves */
int 
do_console(int master, int console) {
  fd_set fdsetw;
  fd_set fdsetr;
  int status;
  char buf[SZ];

  if ((helperpid = fork())) {
    struct sigaction sa;
    /* parent */
    settitle("consoled master (m->c)");
    sa.sa_handler = wrapup;
    sa.sa_flags = 0;
    sa.sa_mask = 0xffffffff;
    if (sigaction(SIGHUP,&sa,NULL) != 0) {
      fprintf(stderr,"sigaction child failed\n");
      exit(-1);
    }
    if (sigaction(SIGTERM,&sa,NULL) != 0) {
      fprintf(stderr,"sigaction child failed\n");
      exit(-1);
    }
    for (;;) {
      FD_ZERO(&fdsetr);
      FD_ZERO(&fdsetw);
      FD_SET(master,&fdsetr);
      status = select(32,&fdsetr,0,0,0);
      
      FD_SET(console,&fdsetw);	
      status = select(32,0,&fdsetw,0,0);
      
      status = read(master,buf,SZ);
      if (status < 0) {
	fprintf(stderr,"read master status: %d, errno: %d\n",status,errno);
	break;
      }
      write(console,buf,status);
    }
    {
      /* wait for a signal */
      sigset_t sig;
      sigemptyset(&sig);
      sigaddset(&sig,SIGCHLD);
      sigaddset(&sig,SIGTERM);
      sigaddset(&sig,SIGHUP);
      sigsuspend(&sig);
    }
    /* never reached */
    return 0;
  } else {
    /* child */
    {
      struct sigaction sa;
      sa.sa_handler = pdone;
      sa.sa_flags = 0;
      sa.sa_mask = 0;
      if (sigaction(SIGUSR2,&sa,NULL) != 0) {
	fprintf(stderr,"sigaction child failed\n");
	exit(-1);
      }
    }
    settitle("consoled helper (c->m)");
    for (;;) {
      FD_ZERO(&fdsetr);
      FD_ZERO(&fdsetw);
      FD_SET(console,&fdsetr);
      status = select(32,&fdsetr,0,0,0);
      
      FD_SET(master,&fdsetw);	
      status = select(32,0,&fdsetw,0,0);
      
      status = read(console,buf,SZ);
      if (status <= 0) {
	fprintf(stderr,"read console status: %d, errno: %d\n",status,errno);
	break;
      }
      status = write(master,buf,status);
      if (status <= 0) {
	fprintf(stderr,"write master status: %d, errno: %d\n",status,errno);
	break;
      }
    }
    close(console);
    close(master);
    exit(0);
  }
  /* never reached */
}


/* returns process id of new process, prompts for username and starts /bin/login */
pid_t 
start_login(int slavefd) {
  pid_t pid;
  static char *username;

  assert(slavefd >= 0);

  if ((pid = fork()) == 0) {
    char *argv[10];
    char *envp[2];
    int argc = 0;
    /* child */
    close(master);
    close(console);

    dup2(slavefd,0);
    dup2(slavefd,1);
    dup2(slavefd,2);
    if (slave > 2) close(slavefd);
    settitle("consoled: login");

    if (setsid() == -1) {
      fprintf(stderr,"setsid failed: errno %d\n",errno);
      exit(-1);
    }
    if (ioctl(0, TIOCSCTTY, (char *)0) < 0) {
      fprintf(stderr,"ioctl slave TIOSCTTY failed: %d,%d\n",pid,errno);
      exit(-1);
    }
    print_banner();
    username = getloginname();
    if (username == NULL) exit(-1);
    envp[0] = "TERM=pc3";
    envp[1] = NULL;
    
    argv[argc++] = "login";
    argv[argc++] = "-p";	/* preserve environment */
    argv[argc++] = "-h";
    argv[argc++] = "localhost";
    argv[argc++] = username;
    argv[argc] = NULL;
    if (execve(LOGIN,argv,envp) == -1) {
      fprintf(stderr,"exec failed\n");
      exit(-1);
    }
    /* never reached */
  }
  return pid;
}

/* taken from: */
/*	$OpenBSD: login.c,v 1.7 1996/09/18 20:39:06 deraadt Exp $	*/
/*	$NetBSD: login.c,v 1.13 1996/05/15 23:50:16 jtc Exp $	*/

#define NBUFSIZ 9
char *
getloginname(void)
{
	int ch;
	char *p;
	static char nbuf[NBUFSIZ];

	for (;;) {
		(void)printf("login: ");
		for (p = nbuf; (ch = getchar()) != '\n'; ) {
			if (ch == EOF) {
				return NULL;
			}
			if (p < nbuf + (NBUFSIZ - 1))
				*p++ = ch;
		}
		if (p > nbuf)
			if (nbuf[0] == '-')
				(void)fprintf(stderr,
				    "login names may not start with '-'.\n");
			else {
				*p = '\0';
				break;
			}
	}
	return nbuf;
}
