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

#include <exos/site-conf.h>

#define TIME_PORT 37
#define DATE_PORT 13

char *remote_hostname = MY_TIME_HOST;
int gettime(struct sockaddr_in *cli_addr, struct sockaddr_in *serv_addr);

int main (int argc, char *argv[]) {
    int i;
    struct sockaddr_in cli_addr, serv_addr;
    struct hostent *localserv;

    memset((char *) &cli_addr,0,sizeof(cli_addr));
    cli_addr.sin_family = AF_INET;
    memset((char *) &serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;

    if (argc >= 2) {
      remote_hostname = argv[1];
    }

    localserv = gethostbyname(remote_hostname);
    if (localserv == (struct hostent *) 0) {
	printf("gethostbyname failed for: %s\n",remote_hostname);
	exit(-1);
    }
    memcpy( (char *) &serv_addr.sin_addr,
	    localserv->h_addr, localserv->h_length);

    //printf("REMOTE IP: %08x\n",*((int *)localserv->h_addr));
    
    for (i = 0; i < 3; i++) {
      if (gettime(&cli_addr,&serv_addr) == 0) {
	//printf("Ok\n");
	return 0;
      } else {
	printf("Retrying...\n");
      }
    }
    printf("Failed\n");
    return -1;
}

#include <arpa/inet.h>
int gettime(struct sockaddr_in *cli_addr, struct sockaddr_in *serv_addr)
{
    int sockfd;
    unsigned long t;
    int status;
    char date[27];
    struct timeval tp;

    struct tm tm;
    time_t t0;
    int settime = 0;

    /* time port returns seconds from 1900 epoch */
    serv_addr->sin_port = htons(TIME_PORT);
	/* use TCP socket, since UDP timeport doesn't seem to work on linux */
    if ((sockfd=socket(PF_INET,SOCK_STREAM,0)) < 0) {
	printf("client can't open stream socket\n");
	exit(-1);
    }

    status = connect (sockfd, (struct sockaddr *)serv_addr, sizeof(*serv_addr));
    if (status < 0) {
       printf ("could not connect to TIME_PORT (ret %d, errno %d)\n", status, errno);
       exit(-1);
    }

    if ((status = write(sockfd,"hello",5)) != 5) {
      printf("problems sending, send returned: %d (expected 5), errno: %d\n",
	     status,errno);
      exit(-1);
    }

    status = read(sockfd,(char *)&t,4);
    close (sockfd);

    /* date port returns 26 char string date  */
    serv_addr->sin_port = htons(DATE_PORT);
    if ((sockfd=socket(PF_INET,SOCK_STREAM,0)) < 0) {
	printf("client can't open datagram socket\n");
	exit(-1);
    }

    status = connect (sockfd, (struct sockaddr *)serv_addr, sizeof(*serv_addr));
    if (status < 0) {
       printf ("could not connect to DATE_PORT (ret %d, errno %d)\n", status, errno);
       exit(-1);
    }

    if ((status = write(sockfd,"hello",5)) != 5) {
      printf("problems sending, send returned: %d (expected 5), errno: %d\n",
	     status,errno);
      exit(-1);
    }

    status = read(sockfd,(char *)date,25);
    close (sockfd);

    date[24] = 0;	/* we clobber the cr */
    close (sockfd);

    /* time will now be in t */

    t = htonl(t);
    t -= 1208988800;	/* uses different epoch HBXX adjusting factor */
    t -= 1000000000; 
    tp.tv_sec = t;
    tp.tv_usec = 0;

    /* Don't cause time to go backwords! */
    /* GROK -- this really should find a way to resync time without */
    /*         reversing local time...                              */
    if (t > time(NULL)) {
       settime = 1;
       if (settimeofday(&tp, NULL) < 0) {
          printf("unable to settimeofday, errno: %d\n",errno);
          exit (-1);
       }
    }

    time(&t0);
    tm = *(localtime(&t0));

    printf("Timezone: %s Daylight savings: %d\n  Our time: %s",
	   tm.tm_zone, tm.tm_isdst,asctime(&tm));
    printf("Their time: %s\n",date);
    if ((settime == 0) || (!memcmp(asctime(&tm),date,24))) {
      return 0;
    } else {
      printf("Times differ\n");
      return -1;
    }
}
