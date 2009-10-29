#include <sys/types.h>
#include <assert.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <memory.h>
#include <malloc.h>
#include <sys/wait.h>
#include <xok/sys_ucall.h>
#include <sys/wait.h>
#include <signal.h>

#include <errno.h>
#include <pwd.h>

/* for b_mnttable */
#include "fd/proc.h"
#include "fd/path.h"
#include <sys/mount.h>
#include <fd/mount_sup.h>

#include <xok/sysinfo.h>
#include <xok/pctr.h>
#define NLS_ALL         0x0001
#define NLS_LONG        0x0002


void print_argv(int argc,char **argv);
int nfs_init();
int ffs_init();
int fork_execvp(const char *path, char *const argv[]);

extern int ae_putc();
int from_file = 0;

void hshell(void);
int print_env();

int check_redirection(int argc,char **argv);
int reset_redirection(void);
int ae_putc(int c) {
  printf("%c",c);
  return c;
}

extern int atoi();
extern int nls();
extern int fstat();
extern int pr_ftable();
extern int nfs_mount_root();

extern int print_error();
extern int ae_pid();
extern int ae_gettick();
extern int ae_getrate();

extern int (*rputs)(char *buf);
extern int remote_puts(char *str);
extern int remote_puts2(char *str);


int b_quit();		

int b_shutdown();
int b_logout();
int b_su();		
int b_exec();		
int b_rm();		
int b_cd();		
int b_cat();
int b_ftable();
int b_mnttable();
int b_nfstable();
int b_nfsflush();
extern int b_nfsncache();
int b_prmount();
int b_mount();
int b_chroot();
int b_test();
int b_setenv();
int b_getenv();
int b_unsetenv();
int b_dump_pt();
int b_whoami();
int b_current();		
int b_stat();		
int b_stat2();		
int b_lstat();		
extern int b_ls();
int b_umount();
int b_banner();
#if 0
int b_rename();
int b_symlink();	
int b_link();
int b_mkdir();		
int b_rmdir();
int b_chmod();		
int b_chown();
int b_df();		
int b_fd();
int b_cp();		
int b_ps();
#endif

int b_pid();		

int b_repeat();
int b_time(int argc, char **argv);

int b_wait();
int b_kill();
int b_waitpoll ();
int commands();
int execute(char *name,int argc, char **argv);

typedef struct func_entry {
  char *name;
  int (*func)();
} func_entry;

func_entry func_tbl[] = {
  {"quit",b_quit},
  {"exit",b_quit},
  {"logout",b_logout},
  {"shutdown",b_shutdown},
  {"reboot",b_shutdown},
  {"cd",b_cd},
  {"icat",b_cat},
  {"istat",b_stat},
  {"istat2",b_stat2},
  {"ilstat",b_lstat},
  {"repeat",b_repeat},
  {"time",b_time},
  {"pid",b_pid},
  {"commands",commands},
  {"ftable",b_ftable},
  {"mnttable",b_mnttable},
  {"nfstable",b_nfstable},
  {"nfsflush",b_nfsflush},
  {"nfsncache",b_nfsncache},
  {"chroot",b_chroot},
  {"printenv",print_env},
  {"setenv",b_setenv},
  {"unsetenv",b_unsetenv},
  {"getenv",b_getenv},
  {"dump_pt",b_dump_pt},
  {"su",b_su},
  {"exec",b_exec},
  {"whoami",b_whoami},
  {"prcurrent",b_current},
  {"kill", b_kill},
  {"banner",b_banner},
  {"?",commands},
#if 0
  {"umount",b_umount},
  {"prmount",b_prmount},
  {"imount",b_mount},
  {"rm",b_rm},
  {"ls",b_ls},
  {"test",b_test},
  {"waitpoll", b_waitpoll},
  {"wait", b_wait},
  {"ps", b_ps},
  {"rename",b_rename},
  {"ln",b_link},
  {"symlink",b_symlink},
  {"cp",b_cp},
  {"mkdir",b_mkdir},
  {"rmdir",b_rmdir},
  {"df",b_df},
  {"fd",b_fd},
  {"chmod",b_chmod},
  {"chown",b_chown},
#endif
  {"",NULL}
};

char prompt[64];
extern char **environ;

int main(int argc,char **argv) 
{
  char name[64];
  char *root;

  if (!gethostname(&name[0],63)) {
	strcpy(prompt,name);
  }
  strcat(prompt,"> ");
  /*   print_argv(argc,argv); */

  if (argc >= 3  && !strcmp(argv[1],"-l")) {
  } else if (argc >= 3  && !strcmp(argv[1],"-f")) {
    int fd;
    from_file = 1;
    if ((fd = open(argv[2],O_RDONLY,0644)) < 0) {
      printf("could not open %s, errno: %d\n",argv[2],errno); 
      exit(-1);
    };
    dup2(fd,0);
    close(fd);
    prompt[0] = 0;
  } else if (argc >= 3  && !strcmp(argv[1],"-c")) {
    argv += 2;
    argc -= 2;
    return execute(argv[0],argc,argv);
  }
  if ((root = getenv("ROOT"))) {
    printf("changing root to %s returned: %d\n",
	   root,chroot(root));
  }

#if 0
    printf("ffs starting...");
    if ((status = ffs_init (0, 0x00000, 0, 0, 1)) != 0) {
      printf (".. disk open failed (%d)\n", status);
    } else {
      printf("done\n");
    }
#endif

    printf("Host: %s\n",name);
    hshell(); 
    return 0;
}

void parse(char *line, char **argv, int *argc, char *argvm);

void hshell(void)
{
#define LINESIZE 255
  char line[LINESIZE];
  char prevline[LINESIZE];
  int argc;
  char *argv[255],*argvm;
  int pid, status;
     
  argvm = (char *) malloc(4096);
  if (!argvm) {
      printf("Error: Could not malloc argvm\n");
      exit(-1);
  }
  printf("Welcome to the hsh 2.1!!!\nType '?' for built-in commmands\n\n");
  
#if 0  
  {
    int i;
    for (i = 0; i < 10; i++) {
      printf ("func[%d] = %p\n", i, func_tbl[i].func);
      /* printf ("func[%d] = %d\n", i, &func_tbl[i]);*/
    }
  }
#endif

  while (1)
    {
      char *aret;
      printf(prompt);
      fflush(stdout);

#if 1
      aret = fgets(line,LINESIZE,stdin);
      /*      printf("line: \"%s\"\n",line); */
      {
	int length;
	length = strlen(line);
	if (line[0] && line[length-1] == '\n') line[length-1] = 0;
      }
      if (aret == (char)NULL) {
	  printf("Done\n");
	  exit(0);
      }
#else
      if (gets(line) == (char)NULL) {
	  printf("Done\n");
	  exit(0);
      }
#endif
      /*       pid = waitpid (-1, &status, WNOHANG); */
      /*       if (pid) printf ("pid %d returned %d\n", pid, status); */

      if (line[0] == (char)NULL) continue;

      parse(line,argv,&argc,argvm);

      if(!strcmp(line,"!!")) {
	  printf ("%s\n",prevline);
	  parse(prevline,argv,&argc,argvm);
      } else {
	  strcpy(prevline,line);
      }

      if (argc == 0) continue;
      if (from_file && argv[0][0] == '#') continue; /* for comments */
      execute(argv[0],argc,argv);

      pid = waitpid (-1, &status, WNOHANG);

      if (pid > 0) 
	if (WIFEXITED(status))
	  printf ("%d terminated with status %d\n", pid, WEXITSTATUS (status));
	else if (WIFSIGNALED(status))
	  printf ("%d terminated with signal %d\n", pid, WTERMSIG(status));
    }
}

int execute(char *name,int argc, char **argv)
{

  int i = 0;

  /*   char *env[] = {(char *)0}; */
  int background = 0;
  int pid, ret;


  if (argc == 0) return 0;

  while(*func_tbl[i].name != 0)
    {
      if (!strcmp(name,func_tbl[i].name)) return (func_tbl[i].func(argc,argv));
      i++;
    }

  if (argv[argc - 1][0] == '&') {
    printf("backgroun process\n");
      background = 1;
      argv[argc - 1] = (char *)0;
      argc--;
  }

  //printf("trying to spawn %s\n",argv[0]);

    
  if (check_redirection(argc,argv)) {

#if 1
    if ((pid = fork_execvp (argv[0],argv)) > 0) {

      //printf (".. fork_execvp returned %d\n",pid);

    } else { 
      printf("%s: Command not found (on all paths).\n",argv[0]);  
      return (-1);
    }
    if (!background && pid != -1) {
      pid = waitpid(pid,&ret,0);
      if (pid > 0) 
	if (WIFEXITED(ret)) {
/*
	  printf ("%d terminated with status %d\n", pid, WEXITSTATUS (ret));
*/
	} else if (WIFSIGNALED(ret)) {
/*
	  printf ("%d terminated with signal %d\n", pid, WTERMSIG(ret));
*/
        }
    }
#else
    if ((pid = vfork())) {
	printf (".. forkv returned %d\n",pid);
      
    } else {
      printf (".. child has pid %d\n",getpid());
      if ((pid = execvp (argv[0],argv))) {
	printf (".. execve returned %d\n",pid);
	assert(0);
      } else { 
	printf("%s: Command not found (on all paths).\n",argv[0]);  
	exit(-1);
      }
    }
#endif
    reset_redirection();
  } else {
    return -1;
  }
  return 0;

}

int commands(int argc, char **argv)
{
  int i = 0;

  while(*func_tbl[i].name != 0)
    printf("%s\n",func_tbl[i++].name);
  return 0;
}

int b_quit(int argc, char **argv)
{
  printf("Thank you for using hsh..\n");
  /*   nunmount_root(); */
  exit(0);}

int b_shutdown(int argc, char **argv)
{
    printf("Shutdown in 0 minutes\n");
    sys_reboot();
    assert(0);
}
int b_logout(int argc, char **argv)
{
    printf("Logging out...");
    /* this should kill any processes in the background also */
    exit(0);
}
#if 1
int b_su(int argc, char **argv)
{
  int uid;
  int status;
  printf("Your user id is: %d\n",getuid());
  if (argc >= 2) {
    uid = atoi(argv[1]);
    if (uid != -1) {
      printf("setting uid to: %d\n",uid);
      status = setuid(uid);
      if (status) perror("setuid failed");
      
    }
  }
  if (argc >= 3) {
    uid = atoi(argv[2]);
    if (uid != -1) {
      printf("setting gid to: %d\n",uid);
      setgid(uid); 
    }
  }
  if (argc != 2 && argc != 3) {
    printf("Usage: su uid [gid]\n");
    return -1;
  }
  return 0;
}
#endif      

int b_exec(int argc, char **argv)
{
  int ret;
  int pid;

  if ((pid = fork ()) < 0) {
    printf ("b_exec: could not fork\n");
    return (0);
  }
  if (pid == 0) {
    /* child */

    argv++;
    argc--;
    ret = execvp(argv[0],argv);
    printf ("b_exec: uh oh! exec returned\n");
  } else {
    printf ("started pid %x\n", pid);
    pid = wait (&ret);
    printf ("pid %d done with ret val %d\n", pid, ret);
  }
  return (0);
}

int b_rm(int argc, char **argv)
{
  int status,i;
  if (argc > 1)
    {for (i = 1; i < argc; i++)
       {status = unlink(argv[i]);
	if (status != 0) perror(NULL);
      }
   } else {
       printf("Usage: rm filename...\n");
   }
  return 0;
}

int b_cd(int argc, char **argv)
{
  int status;
  if (argc != 2)
    printf("Usage: cd directory\n");
  else
    {
      if ((status = chdir(argv[1])) != 0) perror(argv[1]);
    }

  return 0;
}


int b_cat(int argc, char **argv)
{
  int status,i,j;
  char *buf;
  int fd;

  if (argc > 1)
    {for (i = 1; i < argc; i++)
       {fd = open(argv[i],O_RDONLY,0644);
	if (fd < 0) perror(NULL);
	else
	  {
	    buf = (char *) malloc(4096);
	    if (!buf) {
		printf("Error: could not malloc buffer\n");
		return -1;
	    }
	    
	    while((status = read(fd,buf,4096)) > 0)
	      for (j = 0; j < status; j++) printf("%c",buf[j]);
	    
	    free(buf);
	    close(fd);
	  }
      }
   } else {
       printf("Usage: cat filename...\n");
   }
  
  return 0;
}


int b_pid(int argc, char **argv)
{
  printf("Pid: %d\n",getpid());
  printf("Envid: %d\n",__envid);
  return 0;
}


int b_repeat(int argc, char **argv)
{
  int total = 0;
  int status;
  int stop;
  if (argc < 2) 
    {printf("Usage: %s [-nostop] number command...\n",argv[0]);
      return 0;}

  argv = &argv[1];
  argc -= 1;

  stop = 1;
  if (!strcmp("-nostop",argv[0])) {
    stop = 0;
    argv = &argv[1];
    argc -= 1;
  }
  total = atoi(argv[0]);
  argv = &argv[1];
  argc -= 1;
  printf("repeat argc: %d, argv[0]: %s %u times\n",argc,argv[0],total); 
  for (;total > 0; total--) {
      if ((status = execute(argv[0],argc,argv)) != 0 && stop) {
	  printf("execution stopped, program returned non-zero status: %d\n",
		 status);
	  break;
      }
  }
  return 0;
}

int b_time(int argc, char **argv)
{
  pctrval _s;

  int i;
  argc--;
  argv = &argv[1];

  if (argc == 0) 
    {printf("Usage: time commands...\n");
     return 0;}
       
  _s = rdtsc();
  execute(argv[0],argc,argv);
  _s = rdtsc() - _s;
  
  printf("time: %qd msec\n",_s / ((quad_t) __sysinfo.si_mhz*1000));
  printf("for ");
  for (i = 0; i < argc; i++)
    printf("%s ",argv[i]);
  printf("\n");
  return 0;
}




int b_time_old(int argc, char **argv)
{
  int rate = ae_getrate();
  unsigned int initial;
  unsigned int final;
  unsigned int usec = 0,sec = 0,min = 0;
  int i;
  argc--;
  argv = &argv[1];

  if (argc == 0) 
    {printf("Usage: time commands...\n");
     return 0;}
       
  initial = ae_gettick();
  execute(argv[0],argc,argv);
  final = ae_gettick();
  usec = (final - initial) * rate;

  if (usec < 1000000)
    printf("time: 0.%06u seconds ",usec);
  else if (usec < 1000000*60)
    {
      sec = usec / 1000000;
      usec = usec % 1000000;
      printf("time: %u.%06u seconds ",sec,usec);
    }
  else
    {
      sec = usec / 1000000;
      usec = usec % 1000000;
      min = sec / 60;
      sec = sec % 60;
      printf("time: %u:%02u.%06u min:sec ",min,sec,usec);
    }

  printf("for ");
  for (i = 0; i < argc; i++)
    printf("%s ",argv[i]);
  printf("\n");
  return 0;
}


int b_ftable() {
    pr_ftable();
    return 1;
}

int b_nfsflush(int argc, char **argv) {
  extern int nfs_flush_dev(int dev);
  int dev;
  if (argc == 1) {
    char *a[3] = {"foo","flush",0};
    nfs_flush_dev(-1);
    b_nfsncache(3,a);
  } else {
    dev = atoi(argv[1]);
    if (dev < 0) {
      printf("flushing dev: %d\n",dev);
      nfs_flush_dev(dev);
    } else {
      printf("Usage: %s [-#]\n",argv[0]);
      return -1;
    }
   } 
   return 0;
}
int b_nfstable(int argc, char **argv) {
#if 1				/* NFS */
  extern void nfsc_put(void *p);
  extern void *nfsc_get(int dev, int ino);
  extern void nfs_debug(void);
  int choice, dev, ino, p;

  if (argc <= 1) nfs_debug();
  else {
    choice = atoi(argv[1]);
    switch(choice) {
    case 0:
      if (argc != 4) goto bad;
      dev = atoi(argv[2]);
      ino = atoi(argv[3]);
      p = (int)nfsc_get(dev,ino);
      printf("nfsc_get(%d, %d) -> %d\n",dev,ino,p);
      break;
    case 1:
      if (argc != 3) goto bad;
      p = atoi(argv[2]);
      nfsc_put((void*)p);
      printf("nfsc_put(%d)\n",p);
      break;
    default:
    bad:
      printf("%s [[0 d i|1 p]]\n",argv[0]);
      return -1;

    }
  }
#endif
  return 1;
}

int b_mnttable() {
    pr_mounts();
    return 1;
}

int b_dump_pt(int argc, char **argv)
{
  extern void dump_pt();
  dump_pt();
  return 0;
}
int b_current() {
  extern void pr_current(void);
  pr_current();
  return 1;
}

int b_banner(int argc, char **argv)
{
  int i;
  for (i = 0; i < 10; i++) {
    printf("##########################################"
	   "#####################################\n");
  }
	return 0;
}

/* print out process table */
#if 0
int b_prmount() {
  extern void pr_mounts(void);
    pr_mounts();
    return 1;
}
int b_ps () {
     int i;

     printf ("pid\tparent\tstatus\tname\n\n");
     for (i = 0; i < AE_NENV; i++) {
	  if (ProcTable[i].free) {
	       continue;
	  }
	  printf ("%d\t%d\t%c\t%s\n", 
		  i,
		  ProcTable[i].parent,
		  ProcTable[i].status == RUNNING ? 'R' : 'E',
		  ProcTable[i].cmd);
     }

     return (0);
}
/* get exit status and clear proc info for a process */
int b_wait () {
     int status;
     int pid;

     pid = wait (&status);

     printf ("pid %d returned %d\n", pid, status);
     return (0);
}

/* if process has exited, return it's exit stats. Otherwise, return
   immediately. */
#endif
int b_waitpoll () {
     int status;
     int pid;

     pid = waitpid (-1, &status, WNOHANG);
     if (pid == 0)
	  printf ("No terminated child processes.\n");
     else
	  printf ("pid %d returned %d\n", pid, status);
     return (0);
}

#if 0

int b_mount(int argc, char **argv)
{
  int status;
  extern int mount(char *dirto, char *fromname);

  if (argc != 3) printf("Usage: mount host:path|DISK# dir\n");
  else {
    {
      status = mount(argv[2],argv[1]);
     if (status != 0) perror(argv[2]);
   }
  }
  return 0;
}
int b_umount(int argc, char **argv)
{
  int status;
  extern int unmount(char *dirto);
  if (argc != 2) printf("Usage: unmount path\n");
  else {
    status = unmount(argv[1]);
    if (status != 0) perror("unmount");
  }
  return 0;
}
int b_mount(int argc, char **argv)
{
  int status;
  if (argc != 3) printf("Usage: mount host path\n");
  else {
    {
      printf("mounting_root %s:%s\n",argv[1],argv[2]);
      status = nfs_mount_root(argv[1],argv[2]);
     if (status != 0) perror(NULL);
   }
  }
  return 0;
}


int b_test(int argc, char **argv)
{
  b_mount(argc,argv);
  return b_umount(1,(char **)0);

}
#endif

/*
 * here are commented out things that have been removed from the
 * shell itself
 */

#if 1

/* send a process a signal */
int b_kill (int argc, char *argv[]) {
     int pid;
     int signal;

     if (argc < 3) {
	  printf ("usage: %s -signal pid\n", argv[0]);
	  return (0);
     }

     /* make sure signal is of the form -num */
     if (*argv[1] != '-') {
	  printf ("%s: unknown signal\n", argv[0]);
	  return (0);
     }

     signal = atoi (++argv[1]);
     if (signal < 1 || signal > SIGUSR2) {
	  printf ("%s: unknown signal\n", argv[0]);
	  return (0);
     }
     argv += 2;
     while(*argv) {
       pid = atoi (*argv);
       if (pid < 1) {
	 printf ("%s: no such process\n", argv[0]);
       } else if (kill (pid, signal) < 0) {
	 printf ("%s: error sending signal. errno = %d\n", argv[0], errno);
       }
       argv++;
     }

     return (0);
}

int b_whoami(int argc, char **argv)
{
  struct passwd *pw;
  char *user;
  int uid;

  uid = getuid();
  if ((pw = getpwuid(uid)) != (struct passwd *)0) {
    printf("you are user: %s, uid: %d gid: %d\n",pw->pw_name,
	   pw->pw_uid,pw->pw_gid);
    return 0;
  } else {
    printf("warning no passwd entry for uid: %d\n",uid);
    if ((user = getenv("USER"))) {
      printf("your unix env says you want to be user %s\n",user);
    }
    return -1;
  }
}


#endif

#if 0
int b_cp(int argc, char **argv)
{
#define TRANSFER_SIZE 16384
  int fd, fd2;
  char *buf;
  struct stat statbuf;
  int length;
  int tp_flag = 0;
  int v_flag = 0;
  int rate = 0;
  unsigned int initial = 0;
  unsigned int final;
  char *from, *to;

  if (argc < 3) 
    {printf("Usage: cp [-tv] fromfile tofile\n");
     return 0;}

  if (argv[1][0] == '-')
      {
	if (strchr(argv[1],'t'))
	  {tp_flag = 1;
	   rate = ae_getrate();}
	if (strchr(argv[1],'v'))
	  {v_flag = 1;}
	
	argv = &argv[1];}

      
      
  from = argv[1];
  to = argv[2];
  
  buf = (char *)malloc(TRANSFER_SIZE);
  if (!buf) {
      printf("Error: could not malloc buffer\n");
      return -1;
  }

  
  fd = open(from,O_RDONLY,0644);

  if (fd < 0) 
    {free(buf);
     return fd;}

  fd2 = open(to,O_RDWR | O_CREAT | O_TRUNC,0644);

  if (fd2 < 0) 
    {free(buf);
     close(fd);
     return fd2;}

  fstat(fd,&statbuf);
  if (v_flag) printf("copying file %s -> %s, size: %d bytes\n",
		     from,to,statbuf.st_size);
  
  if (tp_flag) initial = ae_gettick();

  while((length = read(fd,buf,TRANSFER_SIZE)))
    {
      if (length < 0) printf("length < 0: %d\n",length);
      length = write(fd2,buf,length); 
    }

  if (tp_flag) 
    {final = ae_gettick();
     printf("Throughput %u Kb/sec, time %u.%06u seconds\n",
	    2*(statbuf.st_size*1000)/((final - initial)*rate),
	    (final - initial)*rate/1000000,
	    (final - initial)*rate % 1000000);
     }

  close(fd);
  close(fd2);
  free(buf);
  return 0;
}



int b_df(int argc, char **argv)
{
  dump_mnttbl();
  return 0;}

int b_fd(int argc, char **argv)
{
  print_fdtable();
  return 0;}



int b_test2(int argc, char **argv)
{

    volatile int ppid = ae_pid();
    int fd;
    char word[] = "hello";
    char buffer[32];
    int count;

    if (argc != 3) {
	printf ("Usage: %s word1 word2\n",argv[0]);
	return (0);
    }

    fork();

    if (ppid == ae_pid()) {
	/* parent */
	printf("OPENING FD 1\n");
	fd = open("/dev/ptyb",O_RDONLY,0644);

	
	if (fd < 0) {
	    printf("fd: %d, errno = %d\n",fd,errno);
	    return (-1);
	}
	printf("PARENT writing: %s\n",argv[1]);
	sleep(2);
	write(fd,argv[1],strlen(argv[1])+1);
	sleep(4);
	count = read(fd,buffer,32);
	printf("PARENT FD: count: %d, buffer: %s\n",count,buffer);

	exit (0);

    } else {
	/* child */
	printf("OPENING FD 2\n");
	fd = open("/dev/ttyb",O_RDWR| O_CREAT | O_TRUNC,0644);
	
	if (fd < 0) {
	    printf("fd: %d, errno = %d\n",fd,errno);
	    exit (-1);
	}
	sleep(4);
	count = read(fd,buffer,32);
	printf("CHILD FD 2: count: %d, buffer: %s\n",count,buffer);
	printf("CHILD writing: %s\n",argv[2]);
	write(fd,argv[2],strlen(argv[2])+1);


    }
	
    return (0);


}
#endif

void parse(char *line, char **argv, int *argc, char *argvm) {
    char **tempv,*temp;

    temp = line;
    tempv = argv;
    *argc = 0;

#if 0
      printf("argv %p argvm %p temp %p tempv %p &argc %p\n",
	     argv, argvm,temp,tempv,argc);
#endif
    while (*temp != (char)NULL)
    {
	while (*temp == ' ' || *temp == '\t') temp++;
	
	*tempv++ = argvm;
	
	if (*temp != ' ' && *temp != '\t' && *temp != '\n' 
	    && *temp != (char)NULL) {(*argc)++;}
	
	while (*temp != ' ' && *temp != '\t' && *temp != (char)NULL)
	{
	    if (*temp == '\"') {
		temp++;
		while(*temp != '\"') {
		    *argvm++ = *temp++;
		}
		temp++;
	    }
	    *argvm++ = *temp++;
	}
	*argvm++ = (char)NULL;
    }
    *argvm = (char)NULL;
    *tempv = argvm;
    argv[*argc] = NULL;
}

void print_argv(int argc,char **argv) {
    int i;
    printf("argc: %d\n",argc);
    for (i = 0; i < argc; i++) {
	printf("argv[%d] = \"%s\"\n",i, argv[i]);
    }
    if (argv[argc] == 0) {
      printf("argv[argc] is equal to NULL\n");
    } else {
      printf("argv[argc] is not equal to NULL (%08x)\n",(int)argv[argc]);
      printf("argv[argc] = \"%s\"\n",argv[argc]);
    }
}

inline void shift_argv(int offset, int length,int *argc,char **argv) {
    int i;
    assert(offset + length - 1 < *argc);
    for (i = offset+length;i < *argc; i++) {
	/* 	printf("argv[%d] = argv[%d]\n",i - length, i); */
	argv[i - length] = argv[i];
    }

    *argc -= length;
    argv[*argc] = (char *)0;
}



static char *filename_input;
static char *filename_output;
static int filename_input_fd;
static int filename_output_fd;

int check_redirection(int argc,char **argv) {
    int i,fd,status;
    int flags = O_RDWR | O_CREAT;
    filename_input = NULL;
    filename_output = NULL;
    for (i = 0; i < argc; i++) {
	if (argv[i][0] == '>') {
	    if (i == (argc - 1) || argv[i + 1][0] == '<') {
		printf("missing file name for output\n");
		return 0;
	    } else {
		if (argv[i][1] == '>') { 
		  flags |= O_APPEND;
		} else {
		  flags |= O_TRUNC;
		}
		filename_output = argv[i + 1];
		shift_argv(i,2,&argc,argv);
		printf("using file: %s as output\n",filename_output);
		i--;
		fd = open(filename_output,flags,0644);
		if (fd < 0) {
		  perror(NULL);
		    return 0;
		}
		filename_output_fd = dup(1);
		if (filename_output_fd < 2) {
		    printf("out of file descriptors\n");
		    close(fd);
		    return 0;
		}
		close(1);
		status = dup(fd);
		assert(status == 1);
		close(fd);
		fcntl(filename_output_fd,F_SETFD,FD_CLOEXEC);
	    }
	} else if (argv[i][0] == '<') {
	    if (i == (argc - 1) || argv[i + 1][0] == '>') {
		printf("missing file name for input\n");
		return 0;
	    } else {
		filename_input = argv[i + 1];
		shift_argv(i,2,&argc,argv);
		printf("using file: %s as input\n",filename_input);
		i--;
		fd = open(filename_input,O_RDONLY,0644);
		if (fd < 0) {
		  perror(NULL);
		    return 0;
		}
		filename_input_fd = dup(0);
		if (filename_input_fd < 2) {
		    printf("out of file descriptors\n");
		    close(fd);
		    return 0;
		}
		close(0);
		status = dup(fd);
		assert(status == 0);
		close(fd);
		fcntl(filename_input_fd,F_SETFD,FD_CLOEXEC);
	    }
	}
    }
    return 1;
}

int reset_redirection(void) {
    int fd;
    
    if (filename_input) {
	close(0);
	fd = dup(filename_input_fd);
	assert(fd == 0);
	close(filename_input_fd);
    }
    if (filename_output) {
	close(1);
	fd = dup(filename_output_fd);
	assert(fd == 1);
	close(filename_output_fd);
    }
    filename_input = 0;
    filename_output = 0;
    return 1;
}

int print_env(int argc,char **argv) {
    char **tmp_env = environ; 
    int envsize = 0;
    printf("ENV: %08x\n",(int)environ);
    while(*tmp_env != (char *)0) {
	printf("[%02d] %s\n",envsize,*tmp_env);
	tmp_env++;
	envsize++;
    }
    printf("envsize: %d\n",envsize);

    return 0;
}

int b_chroot(int argc, char *argv[]) {
  int status;
  switch(argc) {
  case 2:
    status = chroot(argv[1]);
    printf("chroot %s returned: %d\n",argv[1],status);
    break;
  case 3:
    printf("mounting %s:%s\n",argv[2],argv[1]);
    status = nfs_mount_root(argv[2],argv[1]);
    printf("returned: %d\n",status);
    break;
  default:
    printf("Usage: %s path [host]\n",argv[0]);
    return -1;
  }
  return 0;
  
}

int b_setenv(int argc,char **argv) {
  if (argc != 3 && argc != 2) {
    printf("Usage: setenv name value\n");return -1;
  } else if (argc == 2) {
    return setenv(argv[1],"",1);
  } else {
    return setenv(argv[1],argv[2],1);
  }
    return 0;
}
int b_getenv(int argc,char **argv) {
  char *name;
  if (argc != 2) {
    printf("Usage: getenv name\n");return -1;
  } else {
    name = getenv(argv[1]);
    if (name) {
      printf("%s\n",name);
      return 0;
    } else {
      printf("name %s not found\n",argv[1]);
      return -1;
    }
  }
}

int b_unsetenv(int argc,char **argv) {
  if (argc != 2) {printf("Usage: unsetenv name\n");return -1;
  } else {
    unsetenv(argv[1]);
    return 0;
  }
}


void print_stat(char *,int,int ls);
void print_statbuf(char *f, struct stat *statbuf);

int b_stat(int argc, char **argv) {
  argv++;
  if (*argv) {
    while(*argv) {
  int fd;
  printf("VIA OPEN\n");
      if ((fd = open(*argv,0,0)) < 0) printf("could not open: %s errno: %d\n",*argv,errno);
      else {
	struct stat statbuf;
	printf("OPENED\n");
	fstat(fd,&statbuf);
	printf("PRINT_STATBUF\n");
	print_statbuf(*argv,&statbuf);
	printf("closing\n");
	close(fd);
	printf("done\n");
      }
      argv++;
    }
  } else { printf("Usage: istat files...\n");}
  return 0;
}
int b_stat2(int argc, char **argv) {
  argv++;
  if (*argv) {
    while(*argv) {
      print_stat(*argv,-1,0);
	argv++;
	continue;
    }
  } else { printf("Usage: istat files...\n");}
  return 0;
}

int b_lstat(int argc, char **argv) {
  argv++;
  if (*argv) {
    while(*argv) {
      print_stat(*argv,-1,1);
	argv++;
	continue;
    }
  } else { printf("Usage: istat files...\n");}
  return 0;
}
void print_stat(char *f, int fd, int ls) {
  struct stat statbuf;
  memset((char *)&statbuf,0xff,sizeof statbuf);

  printf("sizeof struct stat: %d\n",sizeof(struct stat));

  if (fd == -1) {
    if (((ls == 1) ? lstat(f,&statbuf) : stat(f,&statbuf)) < 0) {
      printf("could not [l]stat file %s\n",f);
      return;
    }
  } else {

    if (fstat(fd,&statbuf) < 0) {
      printf("could not stat fd %d\n",fd);
      return;
    }
  }
  print_statbuf(f,&statbuf);
}
#if 0				/* defined in nfs_read.c */

void print_statbuf(char *f, struct stat *statbuf) {
    printf("file: %s
st_dev:   0x%-8x st_ino:    %-6d     st_mode: 0x%4x st_nlink: %d
st_rdev:  0x%-8x st_uid:    %-6d     st_gid:      %-6d    
st_atime: 0x%-8lx st_mtime:  0x%-8lx st_ctime:    0x%-8lx
st_size:  %-8qd   st_blocks: %-8qd   st_blksize: %5d\n
",
f,
statbuf->st_dev,
statbuf->st_ino,
statbuf->st_mode,
statbuf->st_nlink,
statbuf->st_rdev,
statbuf->st_uid,
statbuf->st_gid,
statbuf->st_atime,
statbuf->st_mtime,
statbuf->st_ctime,
statbuf->st_size,
statbuf->st_blocks,
(int)statbuf->st_blksize);

}  

#endif
