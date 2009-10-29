#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/termios.h>
#include <unistd.h>
#include <memory.h>
#include <grp.h>
#include <err.h>

#include <sys/tty.h>

#ifndef EXOPC
#define kprintf printf
#endif

int openpty(int *amaster, int *aslave, char *name, 
	struct termios *termp, struct winsize *winp);

int master,slave;		/* global fds */

int doshell(int master,int slave);
int test_nonblocking();
int test_blocking();
void print_statbuf(char *f, struct stat *statbuf);

int probefd(void) {
  fd_set fdsetw;
  fd_set fdsetr;
  int status;
  struct timeval timeout = { 0 , 0 }; /* 3 seconds */

  FD_ZERO(&fdsetr);
  FD_ZERO(&fdsetw);
  FD_SET(slave,&fdsetr);
  FD_SET(master,&fdsetr);
  FD_SET(slave,&fdsetw);
  FD_SET(master,&fdsetw);
  status = select(32,&fdsetr,&fdsetw,0,&timeout);
  printf("Select status: %d\n",status);
  if (status) {
    if (FD_ISSET(slave,&fdsetr))  printf("slave read\n");
    if (FD_ISSET(master,&fdsetr)) printf("master read\n");
    if (FD_ISSET(slave,&fdsetw))  printf("slave write\n");
    if (FD_ISSET(master,&fdsetw)) printf("master write\n");
  }
  return status;
}
int test_select() {
  char str[] = "test_select\n";
  char buf[80];
  fd_set fdset;
  int status;
  int what = 0;
  struct timeval timeout = { 2 , 0 }; /* 3 seconds */

  printf("test_select\n");
  printf("write to master sz: %d\n",sizeof(str));
  write (master,str,sizeof(str));
  printf("done\n");
  FD_ZERO(&fdset);
  FD_SET(slave,&fdset);
  status = select(32,&fdset,0,0,&timeout);
  if (status == 0) {
    /* we timeout improperly */
    ioctl(master,TIOCFLUSH,(char *)&what);
    printf("select failed although there was data for slave\n");
    return -1;
  }
  printf("read to slave\n");
  status = read (slave,buf,80);
  printf("done %d\n",status);
  if (status) {buf[status] = 0; printf("buf: %s\n",buf);}


  printf("write to slave sz: %d\n",sizeof(str));
  write (slave,str,sizeof(str));
  printf("done\n");
  FD_ZERO(&fdset);
  FD_SET(master,&fdset);
  status = select(32,&fdset,0,0,&timeout);
  if (status == 0) {
    /* we timeout improperly */
    ioctl(master,TIOCFLUSH,(char *)&what);
    printf("select failed although there was data for master\n");
    return -1;
  }
  printf("read to master\n");
  status = read (master,buf,80);
  printf("done %d\n",status);
  if (status) {buf[status] = 0; printf("buf: %s\n",buf);}

  ioctl(master,TIOCFLUSH,(char *)&what);
  printf("selecting on no slave data\n");
  FD_ZERO(&fdset);
  FD_SET(slave,&fdset);
  status = select(32,&fdset,0,0,&timeout);
  if (status != 0) {
    /* we did not timeout  */
    printf("select succeded although there was data for slave\n");
    return -1;
  }
  printf("Ok\n");
  printf("selecting on no master data\n");
  FD_ZERO(&fdset);
  FD_SET(master,&fdset);
  status = select(32,&fdset,0,0,&timeout);
  if (status != 0) {
    /* we did not timeout  */
    printf("select succeded although there was data for master\n");
    return -1;
  }
  printf("Ok\n");
  


#if 0
  if (fork() == 0) {
    /* child */
  } else {
    /* parent */
  }
#endif
  return 0;
}
int test_raw();

int main(int argc, char **argv) {
  int mfd,sfd;
  int status;
  struct termios t;
  char buffer[80];
  char *tomaster = "2MASTER\n";
  char *toslave = "2SLAVE\n";
  char name[16];
  char *ttynam;
  struct stat sb;
  if (openpty(&mfd,&sfd,name,0,0) < 0) {
    printf("out of ptys\n");
    return -1;
  }
  printf("opened: %s\n",name);
  master = mfd;
  slave = sfd;
  printf("testing ttyname\n");
  if (ttynam = ttyname(master)) {
    name[5] = 'p';
    printf("opened pty: %s, ttyname returned on master %s\n",
	   name,ttynam);
    if (!strcmp(name,ttynam))
      printf("Ok\n"); else printf("Failed ttyname for master\n");
    if (!stat(name,&sb)) {
      print_statbuf(name,&sb);
    } else {
      printf("could not do stat on %s errno: %d\n",name,errno);
    }
  } else {
    printf("could not do ttyname on master fd\n");
  }
  if (ttynam = ttyname(slave)) {
    name[5] = 't';
    printf("opened pty: %s, ttyname returned on slave %s\n",
	   name,ttynam);
    if (!strcmp(name,ttynam))
      printf("Ok\n"); else printf("Failed ttyname for slave\n");
    if (!stat(name,&sb)) {
      print_statbuf(name,&sb);
    } else {
      printf("could not do stat on %s errno: %d\n",name,errno);
    }
    
  } else {
    printf("could not do ttyname on slave fd\n");
  }


  probefd();
  if (test_select() == 0) {printf("test_select ok\n");}
#if 0
  return 0;
  test_nonblocking();
  test_blocking();
  test_raw();
#endif
  if (fork() != 0) {
    printf("going to read slave\n");
    if ((status = read(sfd,buffer,76)) > 0) {
      buffer[status] = 0;
      printf("0read slave: \"%s\"\n",buffer);
    } else {
      printf("0read slave returned: %d, errno: %d\n",status,errno);
    }
  } else {
    printf("parent should be blocking for 3 seconds\n");
    sleep (3);
    printf("writing data...\n");
    write(mfd,"wakeup\n",7);
    printf("wrote data\n");
    exit(0);
  }

#if 1
  printf("setting master and slave nonblocking\n");
  fcntl(mfd,F_SETFL,O_NONBLOCK);
  fcntl(sfd,F_SETFL,O_NONBLOCK);
#endif
#if 0
  assert(tcgetattr(sfd,&t) == 0);
  t.c_lflag &= ~(ICANON);	/* make it raw */
  t.c_cc[VMIN] = 10;
  t.c_cc[VTIME] = 2;
  (void) tcsetattr(sfd, TCSAFLUSH, &t);
#endif

  assert(tcgetattr(mfd,&t) == 0);
  assert(tcgetattr(sfd,&t) == 0);
  printf("echo: %d echonl: %d\n",  t.c_lflag & ECHO,t.c_lflag & ECHONL);
#if 0
  return (doshell(mfd,sfd));
#endif

  probefd();
  if ((status = read(mfd,buffer,80)) > 0) {
    buffer[status] = 0;
    printf("1read master (echo on): \"%s\"\n",buffer);
  } else {
    printf("1read master returned: %d, errno: %d\n",status,errno);
  }
  if ((status = read(sfd,buffer,80)) > 0) {
    buffer[status] = 0;
    printf("2read slave (echo on): \"%s\"\n",buffer);
  } else {
    printf("2read slave returned: %d, errno: %d\n",status,errno);
  }

  if (fork() == 0) {
    /* child */
    write(mfd,tomaster,strlen(tomaster));
    exit(0);
  }
  sleep(2);
  write(sfd,toslave,strlen(toslave));

  if ((status = read(mfd,buffer,80)) > 0) {
    buffer[status] = 0;
    printf("3read master (echo on): \"%s\"\n",buffer);
  } else {
    printf("3read master returned: %d, errno: %d\n",status,errno);
  }
  if ((status = read(sfd,buffer,80)) > 0) {
    buffer[status] = 0;
    printf("4read slave (echo on): \"%s\"\n",buffer);
  } else {
    printf("4read slave returned: %d, errno: %d\n",status,errno);
  }


  assert(tcgetattr(mfd,&t) == 0);
  assert(tcgetattr(sfd,&t) == 0);

  t.c_lflag &= ~(ECHO);
  t.c_lflag |= ECHONL;

  (void) tcsetattr(sfd, TCSANOW, &t);
  printf("echo: %d echonl: %d\n",  t.c_lflag & ECHO,t.c_lflag & ECHONL);

  write(mfd,tomaster,strlen(tomaster));
  write(sfd,toslave,strlen(toslave));

  probefd();

  if ((status = read(mfd,buffer,80)) > 0) {
    buffer[status] = 0;
    printf("5read master (echo off): \"%s\"\n",buffer);
  } else {
    printf("5read master returned: %d\n",status);
  }

  probefd();

  if ((status = read(sfd,buffer,80)) > 0) {
    buffer[status] = 0;
    printf("6read slave: \"%s\"\n",buffer);
  } else {
    printf("6read slave returned: %d\n",status);
  }
  probefd();
    

  (void) close(sfd);
  (void) close(mfd);
  return 0;
    
    

}

int doshell(int master, int slave) {
  int fd,len;
  char buf[80];
    int i;
#ifdef EXOPC
  if ((fd = open("/dev/console",O_RDWR,0)) < 0) {
    printf("could not open /dev/console\n");
    return -1;
  }
#else
  fd = 1;
#endif
    write(master,"foobar\n",7);
  if (fork() == 0) {
    /* child */
    for(i = 0; i < 5 ; i++) {
      printf("reading slave...\n");
      len = read(slave,buf,80);
      printf("SLAVE LEN %d\n",len);
      if (len > 0) {
	if (len <= 0) {
	  printf("negative len\n");
	  exit (0);
	}
	buf[len] = 0;
	printf("SLAVE BUF: %s\n",buf);
      } else {
	printf("-\n");
      }
      sleep(1);
    }
    exit(-1);
  } else {
    /* parent */
    write(master,"1oobar\n",7);
    write(master,"2oobar\n\r",7);
    write(master,"3oobar\n",7);
    write(master,"\004\000\n",3);
    close(master);
    sleep(5);
  }
  exit(0);
}


void print_statbuf(char *f, struct stat *statbuf) {
    printf("file: %s
st_dev:  %5d     st_ino: %5d      st_mode: %5x     st_nlink: %d
st_uid:  %5d     st_gid: %5d      st_rdev: %5d     st_atime: %d
st_size: %5d  st_blocks: %5d   st_blksize: %5d\n
",f,
statbuf->st_dev,
statbuf->st_ino,
statbuf->st_mode,
statbuf->st_nlink,
statbuf->st_uid,
statbuf->st_gid,
statbuf->st_rdev,
statbuf->st_atime,
statbuf->st_size,
(int)statbuf->st_blocks,
(int)statbuf->st_blksize);

}  

