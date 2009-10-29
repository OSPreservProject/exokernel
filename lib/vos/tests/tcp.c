
#include <xok/network.h>
#include <xok/sys_ucall.h>

#include <vos/kprintf.h>
#include <vos/fd.h>
#include <vos/errno.h>
#include <vos/net/iptable.h>

#include <sys/socket.h>
#include <netinet/in.h>


/*
 * test tcp sockets and connections 
 */

int 
main(int argc, char **argv)
{
  int fd, r;
  struct sockaddr_in to;
  struct sockaddr_in self;
  char addr1[4] = {18,26,4,102};
  char selfaddr[4] = {18,26,4,82};
  char mesg[32];
 
  bzero(&self, sizeof(self));
  self.sin_port = htons(12345);
  self.sin_family = AF_INET;
  memcpy(&(self.sin_addr.s_addr),selfaddr,4);

  bzero(&to, sizeof(to));
  to.sin_port = htons(14569);
  to.sin_family = AF_INET;
  memcpy(&(to.sin_addr.s_addr),addr1,4);

  fd = socket(AF_INET,SOCK_STREAM,0);
  printf("socket: fd = %d (should be 3) errno = %d\n",fd,errno);

  r = bind(fd, (struct sockaddr*)&self, sizeof(self));
  printf("bind: r = %d (should be 0), errno = %d\n",r,errno);

  r = connect(fd, (struct sockaddr*)&to, sizeof(to));
  printf("connect: r = %d (should be 0), errno = %d\n",r,errno);

#ifdef __SMP__
  if (fork_to_cpu(1,-1)==0)
#else
  if (fork()==0)
#endif
  {
    r = write(fd, "hello", 5);
    printf("write: r = %d (should be 5), errno = %d\n",r,errno);

    r = read(fd, mesg, 32);
  
    if (r>0)
      mesg[r]='\0';
    printf("read: r = %d (should be 12), errno = %d, mesg = %s\n",r,errno,mesg);
  
    r = close(fd);
    printf("close: r = %d (should be 0), errno = %d\n",r,errno);
  }
  else
    r = close(fd);
 
  return 0;
}


