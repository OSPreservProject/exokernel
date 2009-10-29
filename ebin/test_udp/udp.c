
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <netinet/in.h>
#include <memory.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

int nfs_init(void);
extern int close();

/* use connected datagram socket */
/* #define CONNECT */

/* use tcp */
/* #define TCP */



#ifdef TCP
#ifndef CONNECT
#define CONNECT
#endif /* CONNECT */
#endif /* TCP */

extern int connect(),write(),read();
#ifdef EXOPC
extern int FDFreshProcInit(void);
extern int FDExecInit(void);
#include "assert.h"
void die (void) __attribute__ ((noreturn));
void exit(int value) {
  printf("exit value: %d\n",value);
  die();
}
#endif
#define SERVER_PORT 7

#define BUFSZ 1024		/* largest echo buffer */
char buffer0[BUFSZ];
char *buffer = &buffer0[0];
char sendbuf[BUFSZ];
int sockfd;
int sockfd2;
struct sockaddr_in serv_addr;

int test_string(int sockfd,char *str, int len);


int main (int argc, char *argv[]) {
  int status,i,j,k;
    struct sockaddr_in cli_addr;
    struct hostent *localserv;
    
    char *remote_hostname = "delft";

    printf("#####\n#### TEST-UDP \n#####\n");
#ifdef EXOPC
    printf("FDFreshProcInit()\n");
    FDFreshProcInit();
    printf("done\n");
    printf("FDExecInit()\n");
    FDExecInit();
    printf("done\n");
#endif /* EXOPC */

    
    /* Open Source Socket */
#ifndef TCP
#ifdef CONNECTED
    printf("Using CONNECTED UDP\n");
#else
    printf("Using UDP\n");
#endif
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
#endif




    
    /* Open Source Socket */
#ifndef TCP
#ifdef CONNECTED
    printf("Using CONNECTED UDP\n");
#else
    printf("Using UDP\n");
#endif
    if ((sockfd2=socket(PF_INET,SOCK_DGRAM,0)) < 0) {
	printf("client can't open datagram socket\n");
	exit(-1);
    }
#else
    printf("Using TCP\n");
    if ((sockfd2=socket(PF_INET,SOCK_STREAM,0)) < 0) {
	printf("client can't open stream socket\n");
	exit(-1);
    }
#endif
    printf("got sockfd: %d\n",sockfd2);
    memset((char *) &cli_addr,0,sizeof(cli_addr));
    cli_addr.sin_family = AF_INET;

    printf("binding...");
    if (bind(sockfd2,
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
    if (connect(sockfd2,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
	printf("problems connecting, errno: %d\n",errno);
	exit(-1);
    }
    printf("done\n");
#endif




    
    memset(sendbuf,1,BUFSZ);
    test_string(sockfd,"hector",6);
    test_string(sockfd2,"hector",6);
    printf("done\n");

    printf("testing different lengths\n");
    for(i = 0;  i < BUFSZ - 2; i++) {
      if (test_string(sockfd,sendbuf,i) != 0) {
	printf("FAILED1 on length: %d\n",i);
	return -1;
      }
      if (test_string(sockfd2,sendbuf,i+2) != 0) {
	printf("FAILED1 on length: %d\n",i);
	return -1;
      }
    }

    printf("testing different offsets\n");
    memset(sendbuf,0,BUFSZ);
    for(i = 0;  i < BUFSZ; i++) {
      sendbuf[i] = i & 0xff;
      if (test_string(sockfd,sendbuf,BUFSZ) != 0) {
	printf("FAILED2 on offset: %d\n",i);
	return -1;
      }
      sendbuf[i] = i & 0xfd;
      if (test_string(sockfd2,sendbuf,BUFSZ) != 0) {
	printf("FAILED2 on offset: %d\n",i);
	return -1;
      }
    }


    printf("testing different cycles\n");
    for(i = 1;  i < BUFSZ; i++) {
      for (k = 0; k < 255; k++) {
	sendbuf[k] = (k % i + 1) & 0xff;
      }
      if (test_string(sockfd,sendbuf,BUFSZ) != 0) {
	printf("FAILED3 on cycle: %d\n",i);
	return -1;
      }
      for (k = 0; k < 255; k++) {
	sendbuf[k] = (k % i + 3) & 0xff;
      }
      if (test_string(sockfd2,sendbuf,BUFSZ) != 0) {
	printf("FAILED3 on cycle: %d\n",i);
	return -1;
      }
    }

    printf("DONE SUCCESSFULLY\n");
    printf("closing...");
    close(sockfd); 
    close(sockfd2); 
    printf("done\n");	
    exit(0);
}


int 
test_string(int sockfd, char *str, int len) {
  int flag = 0,i,status;
  struct sockaddr_in dummy_addr;
  int dummy_len;
  memset(buffer,0,BUFSZ);

  /* proper returned status should be checked */
#ifdef CONNECT
#ifndef TCP
    if ((status = send(sockfd,str,len,0)) < 0) {
	printf("problems sending, send returned: %d (expected 5), errno: %d\n",
	       status,errno);
	return -1;
    }
#else
    if ((status = write(sockfd,str,len)) < 0) {
	printf("problems writing, write returned: %d (expected 5), errno: %d\n",
	       status,errno);
	return -1;
    }
#endif /* TCP */
#else
    if ((status = sendto(sockfd,str,len,0,
			 (struct sockaddr *)&serv_addr,sizeof(serv_addr)))
	 < 0) {
	printf("problems sendingto, send returned: %d (expected %d), errno: %d\n",
	       status,len,errno);
	return -1;
    }
#endif /* CONNECT */
    
#ifndef TCP
    status = recvfrom(sockfd,buffer,len,0,&dummy_addr,&dummy_len);
#else
    status = read(sockfd,buffer,len);
#endif
    if (status < 0) {
      printf("errors receiving: %d, errno: %d\n",
	     status,errno);
    }

    for (i = 0; i < len;i++) {
      if (buffer[i] != str[i]) {
	printf("DIFFER sent[%d]= %d (0x%02x) received[%d]= %d (0x%02x)\n",
	       i,str[i],str[i],i,buffer[i],buffer[i]);
	flag++;
	if (flag > 10) {
	  printf("more than 10 differences giving up\n");
	  break;
	}
      }
    }
    if (flag) {
      printf("there were %d differences\n",flag);
      return -1;
    } else {printf("."); return 0;}
}
#if 0

udp length flag (supposed)
252        146 (294),  first 10 bytes differ
19         146 (61) 
42         146 (84) 
offset 415 106 (1066) 
70         106 (112)
224        247 (266)
309        222 (351)
#endif
