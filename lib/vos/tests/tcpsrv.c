
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <vos/errno.h>
#include <vos/fd.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*
 * test tcp server operations
 */

int
main()
{
  int sockfd;
  int n;
  char mesg[128];
  struct sockaddr_in servaddr, cliaddr;
  int clifd, clilen;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(14569);
  
  clilen = sizeof(cliaddr);

  n = bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
  printf("bind: returned %d, should be 0\n",n);
 
  n = listen(sockfd, 5);
  printf("listen: returned %d, should be 0\n",n);

  for(;;)
  {
    printf("waiting for connection to port 14569...\n");
    clifd = accept(sockfd, (struct sockaddr*)&cliaddr, &clilen);

    printf("received tcp connection from port %d of 0x%x\n",
           ntohs(cliaddr.sin_port),ntohl(cliaddr.sin_addr.s_addr));

    n = read(clifd, mesg, 5);
    mesg[n] = '\0';
    kprintf("message: %s\n",mesg);

    n = write(clifd, "acknowledged", 12);
    close(clifd);
  }

  return 0;
}

