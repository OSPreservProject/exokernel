#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <netinet/in.h>
#include <memory.h>
#include <errno.h>
#include <assert.h>


#define P1 0x1
#define P2 0x2
#define P3 0x4


#define SERVER_PORT 13

int main (int argc, char *argv[]) {
    int sockfd, status;
    char buffer[27];
    struct sockaddr_in cli_addr, serv_addr;
    struct sockaddr_in;
    struct hostent *localserv;
    int parent;
    char *remote_hostname = "maastricht.lcs.mit.edu";

    if (argc == 2 && !strcmp(argv[1],"-exec"))
      parent = P3;
    else if (argc == 2 && !strcmp(argv[1],"-go"))
      parent = (P1|P3);
    else 
      parent = (P1|P2);

    if (parent & P1) {
      /* Open Source Socket */
      printf("Using TCP\n");
      if ((sockfd=socket(PF_INET,SOCK_STREAM,0)) < 0) {
	printf("client can't open stream socket\n");
	exit(-1);
      }
      
      printf("got sockfd: %d\n",sockfd);
      memset((char *) &cli_addr,0,sizeof(cli_addr));
      cli_addr.sin_family = AF_INET;
      
      printf("binding...");
      if (bind(sockfd,
	       (struct sockaddr *) &cli_addr,
	       sizeof(cli_addr)) < 0) {
	printf("unable to bind");
	exit(-1);
      }
      printf("done\n");
      
      memset((char *) &serv_addr,0,sizeof(serv_addr));
      serv_addr.sin_family = AF_INET;
      localserv = gethostbyname(remote_hostname);
      
      if (localserv == (struct hostent *) 0) {
	printf("gethostbyname failed for: %s\n",remote_hostname);
	exit(-1);
      }
      
      memcpy( (char *) &serv_addr.sin_addr,localserv->h_addr, localserv->h_length);
      serv_addr.sin_port = htons(SERVER_PORT);
      
      printf("connecting...");
      if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
	printf("problems connecting, errno: %d\n",errno);
	exit(-1);
      }

      dup2(sockfd,10);
      close(sockfd);

    }
    

    if (parent & P2) {
      printf("execing\n");
      execlp(argv[0],argv[0],"-exec",0);
      printf("could not find it\n");
      assert(0);
    }



    if (parent & P3) {
      printf("child\n");
      sockfd = 10;

      printf("done\n");
      if ((status = write(sockfd,"hello",5)) != 5) {
	printf("problems writing, write returned: %d (expected 5), errno: %d\n",
	       status,errno);
	exit(-1);
      }

      printf("reading...");
      status = read(sockfd,buffer,27);
      printf("done read status: %d\n",status);
      
      buffer[26] = 0;
      printf("%s\n",buffer);
      printf("closing...");
      close(sockfd); 
      printf("done\n");	
    }
    exit(0);
}
