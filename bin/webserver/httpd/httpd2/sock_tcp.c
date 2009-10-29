
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>

#include "sock_tcp.h"

#ifdef COMPILE_FOR_BSD
#define StaticAssert	assert
#endif

#ifndef min
#define min(a,b)	(((a) <= (b)) ? (a) : (b))
#endif

static fd_set listen_set;
static fd_set listen_set2;


int sock_tcp_getmainport (int portno)
{
   int fd;
   struct sockaddr_in webaddr;

   if ((fd = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
      fprintf (stderr, "Unable to create the main server TCP socket\n");
      exit(0);
   }

   bzero ((char *) &webaddr, sizeof(webaddr));
   webaddr.sin_family = AF_INET;
   webaddr.sin_addr.s_addr = htonl (INADDR_ANY);
   webaddr.sin_port = htons (portno);

   if (bind (fd, (struct sockaddr *) &webaddr, sizeof (webaddr)) < 0) {
      perror ("Unable to bind to main server socket descriptor");
      exit(0);
   }

   listen (fd, 20);

   return (fd);
}


void sock_tcp_pollforevent (int mainfd, int *connfds, int numconns, int *writefds, int wait)
{
   int i = 0;
   int tmpconns = 0;
   struct timeval tv;
   int ret;

   FD_ZERO (&listen_set);
   FD_ZERO (&listen_set2);

   assert ((mainfd >= 0) && (mainfd < 256));
   FD_SET (mainfd, &listen_set);
   while (tmpconns < numconns) {
      if (connfds[i] != -1) {
         assert ((connfds[i] >= 0) && (connfds[i] < 256));
         FD_SET (connfds[i], &listen_set);
         tmpconns++;
      }
      i++;
   }

   i = 0;
   while (writefds[i] != -1) {
      assert ((writefds[i] >= 0) && (writefds[i] < 256));
      FD_SET (writefds[i], &listen_set2);
      i++;
   }

   tv.tv_sec = 0;
   tv.tv_usec = 0;
   if ((ret = select (256, &listen_set, &listen_set2, NULL, ((wait) ? NULL : &tv))) == -1) {
      perror ("expected break from socket select");
      exit (0);
   } else if (ret == 0) {
      FD_ZERO (&listen_set);
      FD_ZERO (&listen_set2);
   }
}


int sock_tcp_newconn (int mainfd)
{
   return (FD_ISSET (mainfd, &listen_set));
}


int sock_tcp_acceptnewconn (int mainfd)
{
   int newfd;
   struct sockaddr_in clientaddr;
   int clientaddrlen = sizeof (struct sockaddr_in);
   int ret;
/*
printf ("off to accept\n");
*/
   StaticAssert (sizeof (struct sockaddr_in) >= sizeof (struct sockaddr));
   if ((newfd = accept (mainfd, (struct sockaddr *) &clientaddr, &clientaddrlen)) == -1) {
      perror ("Failed to accept new connection");
      exit(0);
   }
/*
printf ("accepted %d\n", newfd);
*/
   if (((ret = fcntl (newfd, F_GETFL, 0)) < 0) || (fcntl (newfd, F_SETFL, (ret | O_NONBLOCK)) < 0)) {
      printf ("sock_tcp_acceptnewconn: Unable to make it nonblocking (errno %d)\n", errno);
      exit (0);
   }
   ret = 16384;
   if (setsockopt (newfd, SOL_SOCKET, SO_RCVBUF, &ret, sizeof (int)) < 0) {
      printf ("sock_tcp_acceptnewconn: failed to set SO_RCVBUF: %s\n", strerror (errno));
      exit (0);
   }
   if (setsockopt (newfd, SOL_SOCKET, SO_SNDBUF, &ret, sizeof (int)) < 0) {
      printf ("sock_tcp_acceptnewconn: failed to set SO_SNDBUF: %s\n", strerror (errno));
      exit (0);
   }

   return (newfd);
}


int sock_tcp_connready (int *connfds, int numconns)
{
   int i = 0;
   int tmpconns = 0;

   while (tmpconns < numconns) {
      if (connfds[i] != -1) {
         tmpconns++;
         if (FD_ISSET (connfds[i], &listen_set)) {
            FD_CLR (connfds[i], &listen_set);
            return (i);
         }
      }
      i++;
   }
   return (-1);
}


int sock_tcp_readycount (int fd)
{
   struct stat statbuf;

printf ("off to fstat\n");
   if (fstat (fd, &statbuf) != 0) {
      perror ("Unable to stat socket descriptor");
      exit(0);
   }
printf ("says that %d bytes are ready\n", (int) statbuf.st_size);

   return ((int) statbuf.st_size);
}


int sock_tcp_getdata (int fd, char *buf, int len)
{
   int ret;
/*
printf ("off to getdata (fd %d, maxlen %d, buf %p)\n", fd, len, buf);
*/
   if (((ret = read (fd, buf, len)) > len) || (ret < 0)) {
      fprintf (stderr, "input size does not equal that indicated by stat: %d (errno %d) != %d\n", ret, errno, len);
      exit(0);
   }
/*
printf ("got %d bytes\n", ret);
*/
   return (ret);
}


void sock_tcp_discarddata (int fd, int len)
{
   char buf[256];
   int discarded = 0;

   while (discarded < len) {
      len += read (fd, buf, min(256, (len - discarded)));
   }
}

