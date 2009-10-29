
/*
 * Copyright (C) 1997 Massachusetts Institute of Technology 
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders. See the file AUTHORS which should have accompanied this software
 * for a list of all copyright holders.
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by the copyright
 * holders listed in the AUTHORS file. The rest of this file is covered by
 * the copyright notices, if any, listed below.
 */

/* if you want the shell reloaded everytime */
#define RELOAD_SHELL

/* if you want /bin in your path */
/*#define PATH_WITH_BIN*/

/* if you want no output on the console from the shell*/
/* #define DEVNULLCHAR */

/* if you wnt more debugging information */
/* #define PTYDEBUG */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include "assert.h"
#include "malloc.h"

#include <signal.h>

#include <xok/sysinfo.h>
#include <xok/sys_ucall.h>
#include <exos/osdecl.h>
#include <pwd.h>
#include "os/procd.h"
#include <xok/env.h>
#include <exos/uwk.h>

void do_child(int fd,int sockfd,char **argv,char *incoming, char *ptyname);

extern int fork_execv(const char *file, char *const argv[]);

int ae_putc(int c) {
  printf("%c",c);
  return c;
}

#define NR_PTY 8

int b_shutdown();

#define RCONSOLED_VERSION "3.0"
#define BUFSIZE 1024

int daemon_port = 5000;

int shell_pid = 0;
    

void 
check_shutdown(int sockfd, int shutfd);

int 
start_shutdown_port(int port);


static struct sockaddr_in remote_from;
static int remote_fromlen = 0;
static int sockfd = 0;


void 
print_buffer(char *buffer,int size) {
    int i;
    printf("buffer: *");
    for (i = 0; i < size; i++) {
	if (buffer[i] == 10) {
	    printf("\\n");
	} else {
	    printf("%c",buffer[i]);
	}
    }
    printf("*\n");
}


static char buffer_send[BUFSIZE];
static char buffer_recv[BUFSIZE];

static int status;



void pchild(int unused) {
  int status,pid;
  kprintf("pchild\n");

  pid = wait (&status);
  if (pid > 0) {
    printf ("pid %d returned %d\n", pid, status);
    exit(0);
  }
}

static void reapchild(int unused) {
  int status,pid;
  //kprintf("reapchild:\n");
  while((pid = wait3(&status,WNOHANG,NULL)) > 0) {
    if (WIFEXITED(status)) {
      kprintf ("  pid %d exited with %d\n", pid,WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
      //kprintf ("  pid %d signaled by %d\n", pid,WTERMSIG(status));
    } else if (WIFSTOPPED(status)) {
      kprintf ("  pid %d stopped by %d\n", pid,WSTOPSIG(status));
    } else {
      kprintf ("  BAD WAIT STATUS pid %d, status: 0x%08x errno: %d\n", pid,status,errno);
    }
  }
}

void reapchildren(void) {
  for(;;) {
    sleep(5);
    reapchild(0);
  }
}


int 
serve_io(int ptyfd, int sockfd) 
{
    fd_set fdset;
    char sequence = 0;
    struct timeval timeout;

    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    /* pr_ftable(); */
    /* printf("serve_io pty %d sockfd %d, envid: %d\n",ptyfd,sockfd,__envid); */
    while(1) {
	while(1) {
	    FD_ZERO(&fdset);
	    FD_SET(ptyfd,&fdset);
	    FD_SET(sockfd,&fdset);
	    /* printf("*S*blocking on a select\n");    */

	    status = select(32,&fdset,0,0,0);
	    if (status) { break;}
	}
	/* 	 	printf("*S*There is something in: sockfd %d ptya %d\n",
			FD_ISSET(sockfd,&fdset),FD_ISSET(ptyfd,&fdset));
			*/
	if (FD_ISSET(sockfd,&fdset)) {

	    /* 	    memset(buffer_recv,'-',BUFSIZE); */
	    status = recvfrom(sockfd,buffer_recv,BUFSIZE,0,0,0); 
	    buffer_recv[status]=0;
#if 0
	    printf("*S*received from rconsole: %d bytes :*",status);
	    print_buffer(buffer_recv,status);

	    printf("*S*buffer from rconsole: %d bytes\n",12);
	    print_buffer(buffer_recv,12);
#endif
	    status = write(ptyfd,buffer_recv,status);
#if 0
	    if (!strncmp("logout",buffer_recv,6) || (status == 0)) {
		printf("someone typed logout or close the other side");
		sleep(2);
		ae_putc('P');

		if (kill (shell_pid, 2) < 0) {
		    printf ("error sending signal. errno = %d\n",errno);
		} else {
		    waitpid(shell_pid,0,0);
		}

		exit(0);
	    }
#endif
	} else {
	    /* 	    printf("*S*Reading...");  */
	    status = read(ptyfd,(char *)buffer_send + 1,BUFSIZE - 1);

	    buffer_send[0] = sequence;

	    /*    printf("*S*to send back to rconsole: \"%s\"\n",buffer); */
	    sequence = (sequence + 1) & 0xFF;
	    sendto(sockfd,buffer_send,status+1,0,(struct sockaddr *)
		   &remote_from,remote_fromlen);

	}
    }
    return 0;
}


int 
initialize_input(char *ptyname) {
    int fd;
    /* open pty's here */
    ptyname[5] = 'p';
#ifdef PTYDEBUG
    printf("start: initialize_input, opening %s\n",ptyname);
#endif
    fd = open(ptyname,O_RDWR,0644);
    if (fd > 0) {
#ifdef PTYDEBUG
	printf("Ok initialize_input\n\n");
#endif
    } else {
	printf("error opening pty: %s, open returned %d, errno: %d\n",ptyname,fd,errno);
	demand(0, nobody was supposed to have this pty open);
    }
       
    return fd;
}


void
initialize_getsi(char **argv, int shutfd) {
  int status;
  struct sockaddr_in cli_addr;
  int fd;
  char ptyname[] = "/oev/ttya";
  int ptyavail;
  char incoming[1000];

#if 0
  int pid;
#endif
  close(0);
  close(1);
  close(2);
  
  ptyavail = 1;
#ifdef PTYDEBUG
  kprintf("we have up to %d ptys\n",NR_PTY);
#endif
  while((fd = open(ptyname,O_RDWR,0700)) < 0) {
      ptyname[8]++;
      if(ptyname[8] > ('a' + NR_PTY - 1)) {
	  /* we are wrapping around because we are out of terminals 
	   * trying again 
	   */
	  ptyname[8] = 'a';
	  sleep(10);
#if 0
	  ptyavail = 0;
	  break;
#endif
      }
  }

  if (fd == 0 && dup2(fd,1) == 1 && dup2(fd,2) == 2) {;
      /*       printf("%s on 0 1 2 Ok\n",ptyname); */
  } else {
      printf("error opening pty: %s, open returned %d (should be 0), errno: %d\n",ptyname,fd,errno);
      demand(0,fd should have opened on 0 and it didnt);
      exit(-1);	
  }

  if ((sockfd=socket(PF_INET,SOCK_DGRAM,0)) < 0) {
      printf("client can't open datagram socket\n");
      printf("We are probably out of sockets, retrying...\n");
      sleep(10);
      return;
  }
  
  memset((char *) &cli_addr,0,sizeof(cli_addr));
  cli_addr.sin_family = AF_INET;
  cli_addr.sin_port = htons(daemon_port);

  if (bind(sockfd,(struct sockaddr *) &cli_addr,sizeof(cli_addr)) < 0)  {
    extern char *__progname;
    kprintf("%s Unable to bind to %08x:%d errno: %d",
	   __progname,0,htons(daemon_port),errno);
    exit(0);
  }


  /*   check_shutdown(sockfd,shutfd); */
  while(1) {
    struct timeval t = { 10, 0};
    int status;
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sockfd,&fdset);
    status = select(32,&fdset,0,0,&t);
    if (status > 0) { break;}
    if (status == -1 && errno == EINTR) continue;
    wait4(0,&status,WNOHANG,0);
  }
  status = recvfrom(sockfd,(char *)&incoming[0],1000,0,
		    (struct sockaddr *)&remote_from,&remote_fromlen);
  demand(status >= 0,recvfrom failed after select);
  incoming[status] = 0;

  if (incoming[0] == 1) {
      ae_putc('S');
      printf("Shutting down\n");
      sendto(sockfd,"\000\000\000\000",4,0,
	     (struct sockaddr *)&remote_from,remote_fromlen);
      b_shutdown();
  }
  if (incoming[0] == 2) {
      ae_putc('O');
      printf("Turning off daemon\n");
      sendto(sockfd,"\000\000\000\000",4,0,
	     (struct sockaddr *)&remote_from,remote_fromlen);
      close(sockfd);
      assert(0);
  }
  close(sockfd);

#ifdef PTYDEBUG
  printf("User: %s just logged on\n",incoming);
#endif

  if ((sockfd=socket(PF_INET,SOCK_DGRAM,0)) < 0) {
    printf("client can't open datagram socket\n");
      exit(0);
  }
  
  memset((char *) &cli_addr,0,sizeof(cli_addr));
  cli_addr.sin_family = AF_INET;
  if (bind(sockfd,(struct sockaddr *) &cli_addr,sizeof(cli_addr)) < 0) {
    printf("unable to bind, exitting");
      exit(-1);
  }

  /* send the new port to client */
  status = sendto(sockfd,(char *)&cli_addr.sin_port,2,0,
	 (struct sockaddr *)&remote_from,remote_fromlen);


#ifdef PTYDEBUG
  printf("rconsoled moving to port: %d\nUsing Rconsole Protocol V3.2\n",
	 htons(cli_addr.sin_port));
#endif
  assert(status == 2);

#ifdef PTYDEBUG
  printf("before fork\n");
#endif
  if (!fork()) { 
    setproctitle("rconsoled-client");
    printf(",");
    do_child(fd,sockfd,argv,incoming,ptyname);

  }
  printf("-");
  sleep(1);
  close(sockfd);
}


/* this is the function that sets up the terminal I/O, the unix env
 * (incoming) and starts the shell (whatever the users sets as
 * XOK_SHELL or default) */

void do_child(int fd,int sockfd,char **argv,char *incoming, char *ptyname) {
  char *shell;
  if (incoming[0]) {
    char *ptr, *user, **e;
    extern char **environ;
    
	ptr = incoming;
	while(*ptr) {
	  /* kprintf("putenv %s\n",ptr); */
	  putenv(ptr);
	  ptr += strlen(ptr) + 1;
	}
	e = environ;
	while(*e != (char *)0) {
	  /* 	  kprintf("env: %s\n",*e); */
	  e++;
	}
	if ((user = getenv("USER"))) {
	  char *uhome;
	  struct passwd *pw;

	  if ((uhome = getenv("HOME"))) {
	    /*kprintf("HOME env set chdir %s\n",uhome);*/
	    if (chdir(uhome) != 0) {
	      printf("could not chdir into %s\n",uhome);
	      };
	  }

	  if ((pw = getpwnam(user)) != (struct passwd *)0) {
	    if (!uhome) {
	      char home[20];
	      home[0] = '/';
	      strcpy(home + 1,user);
	      if (chdir(home) != 0) {
		printf("could not chdir into %s\n",home);
		}
	      setenv("HOME",home,1);
	    }
	    setuid(pw->pw_uid); 
	  }
	}
	setenv("PATH",".",0);
      } else {
	setenv("USER","hbriceno",1);
	setenv("PATH","/:.",0);
	setenv("HOME","/",1);
      }



      /* child */
#ifdef PTYDEBUG
      printf("before initialize_input\n");
#endif
      fd = initialize_input(ptyname);
      /* spawn new process */
      fcntl(sockfd,F_SETFD,FD_CLOEXEC);
      fcntl(fd,F_SETFD,FD_CLOEXEC);
      if (!(shell = getenv("SHELL"))) {
	shell = argv[0];
      } else {
	argv[0] = shell;
      }
      printf("starting shell %s for user %s\n",shell,getenv("USER"));
#if 1
      do {
	struct stat sb;
	if (stat(shell,&sb) < 0) {
	  kprintf("cound not start: %s, exiting\n",shell);
	}
      } while(0);
#endif
      printf("(%d) starting process: ",__envid);
      shell_pid = fork_execv(shell,argv);
      printf("shell_pid %d.\n",shell_pid);
      chdir("/");
      /* this process now becomes the new process io server */
      if (shell_pid < 0) {
	  printf("cound not start: %s, errno: %d\n",shell,errno);
	  printf("make sure %s is accessible\n",shell);
	  do {
	    char buf[255];
	    buf[0] = 0;		/* sequence */
	    sprintf(&buf[1],"cound not start: %s, errno: %d
makesure is accessible\n",shell,errno);
	    sendto(sockfd,
		   buf,strlen(&buf[1])+2,0,(struct sockaddr *)
		   &remote_from,remote_fromlen);
	  } while(0);
	  fflush(stdout);
	  exit(0);
      }
      serve_io(fd,sockfd);
      printf("shell pid: %d done\n",shell_pid);
      demand(0,PROCESS ENDED);
}

static void
dirty_death_check(u_quad_t nothing)
{
  int procd_iterator = init_iter_proc_p();
  proc_p p;
  struct Env *e;

  while ((p = iter_proc_p(&procd_iterator))) {
    if (p->p_stat == SZOMB) continue;
    e = &__envs[envidx(p->envid)];
    if (e->env_status != ENV_OK ||
	e->env_id != p->envid) {
      proc_pid_exit(p->p_pid, (W_EXITCODE(0, SIGKILL)));
    }
  }
}


int 
main (int argc, char *argv[])
{
#ifdef STARTUPSHELL
    char *program = STARTUPSHELL ;
#else
    char *program = "/hbriceno/hsh";
#endif
    char *newargv[] = {program,0,0,0};
    char **finalargv;
    int shutfd =9;
    /* REMAP stdout and stderr (the FILE *) 
       to another filedescriptor */
    {
      extern void setup_net(void);
      setup_net();
    }

    if (fork () == 0) {
      extern void arpd_main (void);
      arpd_main ();
    }

    sleep (1);			/* give arpd a chance to start */

    {
      extern void get_nfs_root (void);
      get_nfs_root ();
    }

    {
      extern void set_hostname ();
      set_hostname ();
    }

    /* check for a newer kernel */
    {
      extern void checkkern(void);
      checkkern();
    }

    /* execute the RCLOCAL(/etc/rc.local) script(or program) */
#ifdef RCLOCAL
    {
      struct stat sb;
      if (stat(RCLOCAL,&sb) == 0) {
	sys_cputs("Spawning ");
	sys_cputs(RCLOCAL);
	sys_cputs("\n");
	system(RCLOCAL);
      }
    }
#endif /* RCLOCAL */

    //sys_cputs ("welcome to rconsoled\n");
    /* setup child reaper */
    signal(SIGCHLD,reapchild);
    dup2(1,14);
    dup2(2,15);
    fcntl(14,F_SETFD,FD_CLOEXEC);
    fcntl(15,F_SETFD,FD_CLOEXEC);
    __sF[1]._file = 14;
    __sF[2]._file = 15;
    setbuf(stdout,(char *)NULL);	/* make stdout unbuffered */
    setbuf(stderr,(char *)NULL);	/* make stdout unbuffered */

    /* if first process start the shell otherwise start with
     * arguments of rconsoled 
     */
    if (argc > 1) {
    
	if (argc < 2) {
	    printf("Usage: %s shell [port]\n",argv[0]);
	    exit(-1);
	}
	newargv[0] = argv[1];
	if (argc > 2) {
	    daemon_port = atoi(argv[2]);
	}
    }
    finalargv = newargv;
    
    //kprintf("Rconsoled version %s  ",RCONSOLED_VERSION);
    //kprintf("Using Daemon port %d\n",daemon_port);

    /* check whenever a process dies to see if it wasn't clean */
    wk_register_extra((int (*)(struct wk_term *,u_quad_t))wk_mkenvdeath_pred,
		      dirty_death_check, 0, UWK_MKENVDEATH_PRED_SIZE);
    
    /* benjie: there could be a race condition: a dead env gets cleaned, then
     * we try to register this predicate while another env is cleaned...
     * therefore we need to also wakeup regularly to check */
    wk_register_extra((int (*)(struct wk_term *,u_quad_t))wk_mkusleep_pred,
		      dirty_death_check, 1000000, UWK_MKSLEEP_PRED_SIZE);

    while(1) {
	initialize_getsi(finalargv,shutfd);
    }

    return 0;
}



/* binds a socket to port 10000 */
int 
start_shutdown_port(int port) {
    int sockfd;
    int status;
    struct sockaddr_in cli_addr;
    char buffer[16];
    assert(port > 0);

    if ((sockfd=socket(PF_INET,SOCK_DGRAM,0)) < 0) {
	printf("client can't open datagram socket\n");
	exit(0);
    }

    if (sockfd < 3) { 
	if (dup2(sockfd,7) != 7) {
	    printf("warning: could not move shutdown port to another fd\n");
	    close(sockfd);
	    return 0;
	} else {
	    sockfd = 7;
	}
    }
  
    memset((char *) &cli_addr,0,sizeof(cli_addr));
    cli_addr.sin_family = AF_INET;
    cli_addr.sin_port = htons(port);
  
    if (bind(sockfd,
	     (struct sockaddr *) &cli_addr,
	     sizeof(cli_addr)) < 0)  {
	printf("Unable to start shutdown port 10000, continuing.\n");
	close(sockfd);
	return 0;
    }
    printf("done");
    status = recvfrom(sockfd,buffer,16,0,0,0); 
    printf("Shutting down...");
    exit(0);
}
  
void 
check_shutdown(int sockfd,int shutfd) {

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sockfd,&fdset);
    FD_SET(shutfd,&fdset);
    
    if (shutfd == 0) return;

    select(32,&fdset,0,0,0);
    if (FD_ISSET(shutfd,&fdset)) {
	/* shutdown */
	printf("Shutting down\n");
	*(int *)0 = 1;
	
	exit(0);
    }
}


int 
b_shutdown()
{
    printf("Shutdown in 0 minutes\n");
    sys_reboot();
    assert(0);
}
