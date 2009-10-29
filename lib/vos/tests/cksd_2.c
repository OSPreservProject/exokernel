
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <vos/errno.h>
#include <vos/fd.h>


#define dprintf if (0) kprintf


int cks(char *s, int l)
{
  int i, sum = 0;
  for(i=0; i<l; i++)
    sum += s[i];
  return sum;
}


/*
 * check sum daemon: receives a string, return a simple checksum calculated by
 * adding int value of all chars together.
 */

int
main(int argc, char *argv[])
{
  int sockfd;
  int n;
  int port;
  int lastcpu = 0;
  char mesg[128];
  struct sockaddr_in servaddr, cliaddr;
  int clifd, clilen;

  if (argc != 2)
  {
    printf("usage: cksd <port>\n");
    return -1;
  }
  port = atoi(argv[1]);

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);
  
  clilen = sizeof(cliaddr);

  n = bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
  dprintf("bind: returned %d, should be 0\n",n);
 
  n = listen(sockfd, 5);
  dprintf("listen: returned %d, should be 0\n",n);
  
  fd_protect(sockfd);

  for(;;)
  {
    dprintf("waiting for connection to port %d...\n", port);
    clifd = accept(sockfd, (struct sockaddr*)&cliaddr, &clilen);
    dprintf("received tcp connection from port %d of 0x%x\n",
           ntohs(cliaddr.sin_port),ntohl(cliaddr.sin_addr.s_addr));

    if (lastcpu == 0) lastcpu = 1;
    else lastcpu = 0;

#ifdef __SMP__
    if (fork_to_cpu(lastcpu, -1) == 0) // child, on cpu 1, update 
#else
    if (fork() == 0) // child
#endif
    {
      dprintf("child: reading descriptor %d\n", clifd);
      n = read(clifd, mesg, 32);
      mesg[n] = '\0';
      dprintf("message: %s (%d)\n",mesg,n);
      n = cks(mesg, n);
      dprintf("writing: %d\n",n);
      n = write(clifd, &n, sizeof(int));
    
      close(clifd);
      exit(0);
    }
    else // parent
    {
      dprintf("parent: close new descriptor %d\n", clifd);
      close(clifd);
      dprintf("parent: new descriptor %d closed\n", clifd);
    }
  }
  return 0;
}

