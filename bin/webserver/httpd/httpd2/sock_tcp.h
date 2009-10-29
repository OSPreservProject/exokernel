
#ifndef __SOCK_TCP_H__
#define __SOCK_TCP_H__

int sock_tcp_getmainport (int portno);
void sock_tcp_pollforevent (int mainfd, int *connfds, int numconns, int *writefds, int wait);
int sock_tcp_newconn (int mainfd);
int sock_tcp_acceptnewconn (int mainfd);
int sock_tcp_connready (int *connfds, int numconns);
int sock_tcp_readycount (int fd);
int sock_tcp_getdata (int fd, char *buf, int len);
void sock_tcp_discarddata (int fd, int len);

#endif	/* __SOCK_TCP_H__ */

