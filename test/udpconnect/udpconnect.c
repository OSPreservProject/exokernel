#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <netinet/in.h>
#include <memory.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#define PR printf("LINE: %d\n",__LINE__)

/* make sure you set this */
#define SRC_ADDR {18,26,4,33}
#define SRC_PORT 1234		/* for fully connected op */
#define DST_ADDR {18,26,4,40}
#define DST_PORT 13	/* date port to get something back */
#define GETOFF(addr,off) ((unsigned char [4])addr)[off]
int main() {
  int good = 0;
  struct sockaddr_in s;
  int sockfd;
  char buf[27];
  int len;
  printf("make sure you set the SRC_ADDR (%d.%d.%d.%d) is right.\n ",
	 GETOFF(SRC_ADDR,0),
	 GETOFF(SRC_ADDR,1),
	 GETOFF(SRC_ADDR,2),
	 GETOFF(SRC_ADDR,3));
  printf("it is defined inside udpconnect.c\n");

  s.sin_port = htons(DST_PORT);
  memcpy((char *)&s.sin_addr,(char [4])DST_ADDR,4);
  s.sin_family = AF_INET;

  printf("HALF CONNECTED\n");

  if ((sockfd=socket(PF_INET,SOCK_DGRAM,0)) < 0) {
    printf("client can't open datagram socket\n");
    return(-1);
  }
  if (fcntl(sockfd,F_SETFL,O_NDELAY) < 0) {
    printf("client can't connect datagram socket: %d\n",errno);
    return(-1);
  }
  if (connect(sockfd,(struct sockaddr *)&s,sizeof(s)) < 0) {
    printf("client can't connect datagram socket: %d\n",errno);
    return(-1);
  }
  if (send(sockfd,"foo",3,0) < 0) {
    printf("client can't send datagram socket: %d\n",errno);
    return(-1);
  }

  sleep(2);
  memset(buf,0,26);
  if ((len = recv(sockfd,&buf[0],26,0)) < 0) {
    good = -1;
    printf("client can't recv datagram socket: %d\n",errno);
    printf("Failed\n");
  } else {
    buf[26] = 0;
    printf("RECV %d: %s\n",len,buf);
  }
  close(sockfd);



  printf("FULLY CONNECTED\n");

  if ((sockfd=socket(PF_INET,SOCK_DGRAM,0)) < 0) {
    printf("client can't open datagram socket\n");
    return(-1);
  }
  if (fcntl(sockfd,F_SETFL,O_NDELAY) < 0) {
    printf("client can't connect datagram socket: %d\n",errno);
    return(-1);
  }

  /* setting local address */
  s.sin_port = htons(SRC_PORT);
  memcpy((char *)&s.sin_addr,(char [4])SRC_ADDR,4);
  s.sin_family = AF_INET;


  if (bind(sockfd,(struct sockaddr *)&s,sizeof(s)) < 0) {
    printf("client can't bind datagram socket: %d\n",errno);
    return(-1);
  }

  s.sin_port = htons(DST_PORT);
  memcpy((char *)&s.sin_addr,(char [4])DST_ADDR,4);
  s.sin_family = AF_INET;

  if (connect(sockfd,(struct sockaddr *)&s,sizeof(s)) < 0) {
    printf("client can't connect datagram socket: %d\n",errno);
    return(-1);
  }
  if (send(sockfd,"foo",3,0) < 0) {
    printf("client can't send datagram socket: %d\n",errno);
    return(-1);
  }

  sleep(2);
  memset(buf,0,26);
  if ((len = recv(sockfd,&buf[0],26,0)) < 0) {
    good = -1;
    printf("client can't recv datagram socket: %d\n",errno);
    printf("Failed\n");
  } else {
    buf[26] = 0;
    printf("RECV %d: %s\n",len,buf);
  }
  close(sockfd);

  if (good == 0) printf("Ok\n"); else printf("Failed\n");
  return good;
}
