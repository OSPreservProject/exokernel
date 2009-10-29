#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <netinet/in.h>
#include <memory.h>
#include <errno.h>

extern int close();

/* use connected datagram socket */
/* #define CONNECT */

/* use tcp */
#define TCP



#ifdef TCP
#ifndef CONNECT
#define CONNECT
#endif /* CONNECT */
#endif /* TCP */

#define SERVER_PORT 13
int main (int argc, char *argv[]) {
    int sockfd, status;
    char buffer[27];
    struct sockaddr_in cli_addr, serv_addr;
    struct sockaddr_in;
    struct hostent *localserv;
    
    char *remote_hostname = "maastricht.lcs.mit.edu";
    
    /* Open Source Socket */
#ifndef TCP
    printf("Using UDP\n");
    if ((sockfd=socket(PF_INET,SOCK_DGRAM,0)) < 0) {
	printf("client can't open datagram socket\n");
	exit(-1);
    }
#else
    printf("Using TCP\n");
    if ((sockfd=socket(PF_INET,SOCK_STREAM,0)) < 0) {
	printf("client can't open stream socket\n");
	exit(-1);
    }
#endif
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
    
#ifdef CONNECT
    printf("connecting...");
    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
	printf("problems connecting, errno: %d\n",errno);
	exit(-1);
    }
    printf("done\n");
#ifndef TCP
    if ((status = send(sockfd,"hello",5,0)) != 5) {
	printf("problems sending, send returned: %d (expected 5), errno: %d\n",
	       status,errno);
	exit(-1);
    }
#else
    if ((status = write(sockfd,"hello",5)) != 5) {
	printf("problems writing, write returned: %d (expected 5), errno: %d\n",
	       status,errno);
	exit(-1);
    }
#endif /* TCP */
#else
    if ((status = sendto(sockfd,"hello",5,0,
			 (struct sockaddr *)&serv_addr,sizeof(serv_addr)))
	 != 5) {
	printf("problems sending, send returned: %d (expected 5), errno: %d\n",
	       status,errno);
	exit(-1);
    }
#endif /* CONNECT */
    
#ifndef TCP
    printf("receiving...");
    status = recvfrom(sockfd,buffer,27,0,0,0);
    printf("done\n");
#else
    printf("reading...");
    status = read(sockfd,buffer,27);
    printf("done\n");
#endif
    buffer[26] = 0;
    printf("%s\n",buffer);
    printf("closing...");
    close(sockfd); 
    printf("done\n");	
    exit(0);
}
