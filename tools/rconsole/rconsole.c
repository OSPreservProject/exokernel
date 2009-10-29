
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

/* uses a second port after establishing a connection */

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <memory.h>
#include <strings.h>
#include <sys/time.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
/* if you dont want timeouts, set TIMEOUT to 0 */
#define TIMEOUT 0		/* in hours, the rconsole will never run
				   idle longer than this time (in hours) */

#define BUFSIZE 8193		/* + 1 byte for sequence number */
#define LINESIZE 255
#define RCONSOLE_VERSION "3.0"

void print_usage(void);

int child_pid = 0;
struct sockaddr_in cli_addr, serv_addr;
struct sockaddr dummy_addr;
int dummy_len;
int sockfd;



void controlc() {
    printf("Logging out (via Control-C possibly)\n");
    if (child_pid != 0) {
 	sendto(sockfd, "logout\n", 8,0,(struct sockaddr *)&serv_addr, sizeof(serv_addr));
 	sendto(sockfd, "exit\n", 8,0,(struct sockaddr *)&serv_addr, sizeof(serv_addr));
	printf("killing child: %d, my pid: %d\n",child_pid,getpid()); 
	if (kill(child_pid,1) == -1) {
	    printf("KILL FAILED for pid: %d\n",child_pid);
	} else {
	  /* No need to wait since we have sigchld ignore
	    int statusp;
	    int status;
	    status = wait(&statusp);
	    printf("wait returned: %d, statusp: %d\n",status,statusp); 
	    */
	}
    }
    exit(0);
}

void timeout_handler() {
    printf("%d hours have passed since the rconsole.ng started... timing out\n",TIMEOUT);
    controlc();
}

void nop(void) {
    return;
}

int shutdown_flag = 0;
int log_flag = 0;
int logfile;
int off_flag = 0;
int seq_flag = 1;

extern char **environ;

#define TOTAL 1000

char *setupenv(int *length) {
  char **e;
  int total,l;
  char *xokenv,*xokenvp;

  e = environ;
  total = 0;
  if (!(xokenv = malloc(TOTAL))) {
    printf("could not malloc %d\n",TOTAL);
    exit(-1);
  }
  xokenvp = xokenv;

  if (!getenv("XOK_USER")) {
    char *user;
    int len;
    if ((user = getenv("USER"))) {
      strcpy(xokenvp,"USER=");
      strcat(xokenvp,user);
      len = 5 + strlen(user) + 1;
      total += len;
      xokenvp += len;
    } else {
      printf("you do not even have your USER env variable set!!\n");
    }
  }
  while(*e != (char *)0) {
    if (!strncmp(*e,"XOK_",4)) {
      l = strlen(*e);
      l = l - 4 + 1;
      if (total + l < TOTAL) {
	/*printf("copying: %s\n",*e);*/
	strcpy(xokenvp,*e + 4);
	xokenvp += l;
	total += l;
      }
    }
    e++;
  }
  *xokenvp = 0;
  *length = total + 1;
  return xokenv;
}

int main(int argc, char **argv)
{
    int sent, received;
    long mask;
    int port = 5000;		/* default rconsole port */
    struct timeval block = { 1, 0};
    struct hostent *remotehost;
    char *host = getenv("AEGIS_HOST");
    char line[LINESIZE];
    char *buffer = (char *)malloc(BUFSIZE);
    char sequence = 0;
    int i;
    char *word,*nextword;

    setbuf(stdout,NULL);	/* set stdout unbuffered */
    /* Open Source Socket */

    if (!strcmp(argv[0],"rshutdown")) {shutdown_flag = 1;}
    for(i = 1; i < argc; i++) {
      word = argv[i];
      nextword = (i+1 >= argc) ? 0 : argv[i+1];
      if (!strcmp(word,"-shutdown")) {
	shutdown_flag = 1;
      } else if (!strcmp(word,"-help") || !strcmp(word,"--help")) {
	print_usage();
      } else if (!strcmp(word,"-off")) {
	off_flag = 1;
      } else if (!strcmp(word,"-noseq")) {
	seq_flag = 0;
      } else if (!strcmp(word,"-log")) {
	if (!nextword) print_usage();
	log_flag = 1;
	if ((logfile = open(nextword,O_APPEND|O_WRONLY|O_CREAT,0664)) == 0) {
	  printf("could not open: %s\n",nextword);
	  exit(-1);
	}
	i++;
      } else {
	host = word;
	if (nextword) {
	  port = atoi(nextword);
	  i++;
	}
      }
    }
    
    if ((sockfd=socket(PF_INET,SOCK_DGRAM,0)) < 0) 
	printf("Client can't open datagram socket\n");
    
    memset((char *) &cli_addr,0,sizeof(cli_addr));
    cli_addr.sin_family = AF_INET;
    
    if (bind(sockfd,(struct sockaddr *) &cli_addr,sizeof(cli_addr)) < 0)
    {printf("Unable to bind"); return -1;}
    
    memset((char *) &serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    
    remotehost = gethostbyname(host);
    
    if (remotehost == 0) {printf ("Unknown host: %s\n",host); exit(1);}
    
    memcpy( (char *) &serv_addr.sin_addr,remotehost->h_addr, 
	   remotehost->h_length);
    serv_addr.sin_port = htons(port);
    
    printf("Using host: %s port: %d\n",host,port);
    
    do {
	mask = 1 << sockfd; /* nice way for selecting on one socket! */
	if (shutdown_flag) {
	  printf("sending shutdown commmand...\n");
	  sent = sendto(sockfd, "\001", 1,
			0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	  
	} else if (off_flag) {
	  printf("sending off daemon commmand...\n");
	  sent = sendto(sockfd, "\002", 1,
			0,(struct sockaddr *)&serv_addr, sizeof(serv_addr));
	} else {
	    char *user;
	    int length;
#if 0
	    user = getenv("USER");
	    length = strlen(user) + 1;
#else 
	    user = setupenv(&length);
#endif
	    sent = sendto(sockfd, user, length,
			  0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	}
	select(32,(fd_set *)&mask,0,0,&block);
    } while(!mask);
    
    received = recvfrom(sockfd,buffer,4,0,&dummy_addr,&dummy_len);  
    if (*((int *)buffer) == 0) {printf("done\n");exit(0);}
    do {
	unsigned short port;
	memcpy(&port,buffer,2);
	printf("rconsole version %s, rconsolec moving to port: %d\n",
	       RCONSOLE_VERSION,htons(port));
	serv_addr.sin_port = port;
    } while(0);
    if ((child_pid = fork())) {
	signal(SIGINT,controlc);
	signal(SIGTERM,controlc);
	signal(SIGCHLD,SIG_IGN);

	signal(SIGALRM,timeout_handler);
	alarm(TIMEOUT*3600);

	/* parent */
	printf("Child pid: %d\n",child_pid);
	while(1) {
	    int length;
	    char *ret;

	    memset(&line[0],0,LINESIZE);
	    ret = fgets(line,LINESIZE,stdin);
	    /* 	    printf("line: \"%s\"\n",line); */
	    length = strlen(line) + 1;
	    if (line[length-1] == '\n') {line[length-1] = 0; length--;}
	    if (ret == NULL) {
		printf("Type Control-C to quit\n\n");
		close(0);
		while(1) {sleep(1000);}
	    }
	    
	    if (!strcmp(line,"clear")) 
	    {system("clear"); 
	     sent = sendto(sockfd, "\n", 2, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	     /* so that the prompt moves */
	     continue;}
	    
	    /* replace NULL char by newline (gets does not retain the newline) */
	    line[length - 1] = '\n';
	    line[length] = 0;
#if 0
	    printf("send line:\n*");
	    for (i = 0; i < length; i++) {
		putchar(line[i]);
	    }
	    printf("* (length: %d)\n",length);
#endif
				/* 	    sleep(1);  */
	    sent = sendto(sockfd, line, length,
			  0,(struct sockaddr *)&serv_addr, sizeof(serv_addr));
	    if (log_flag) {write(logfile,line,length);}

	    /* put back the NULL so that strncmp works safely */
	    line[length - 1] = (char )NULL;
	    if (!strcmp(line,"logout")) break;
	    if (!strcmp(line,"shutdown")) {sleep(1);break;}
	}
	controlc();
    } else {
	signal(SIGINT,SIG_IGN);
	signal(SIGTERM,SIG_IGN);
	close(0);
	while(1) {
	    /* Receiving loop */
	    received = recvfrom(sockfd,buffer,BUFSIZE,0,&dummy_addr,&dummy_len);
	    if (sequence != buffer[0] && seq_flag) {
		printf("* Received a packet out of sequence.\n");
		sequence = buffer[0]; 
	    }
	    
	    sequence = (sequence + 1) & 0xFF;
	    write(1,(char *)buffer + seq_flag,received-seq_flag);
	    /* 	  for (i = 0; i < received ; i++) putchar(buffer[i]); */
	    if (log_flag) {
	      write(logfile,(char *)buffer + seq_flag,received-1);
	    }
	}
    }
    return(0);
}

void print_usage(void)
{
  printf("
Usage: rconsole.ng [-help] [-shutdown|-off] [-log file] [host [port]]
If no host is used, rconsole will use environment variable AEGIS_HOST
as the host, it uses default port 5000, which can be overriden by the 
enviromental variable (green) AEGIS_PORT or at the command line.
-help: prints this message
-off: turns off the rconsoled at the exohost so no more connections are allowed
-shutdown: shuts down the exomachine
-log file: logs onto a file your session (equivalent to using script)
-noseq: turns off sequence on packets for use with old xok\n");
    exit (1);
}
