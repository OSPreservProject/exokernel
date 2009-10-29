#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int check_ip(char *ip,int converted);
int try(char *name);


int 
main(int argc,  char **argv)
{
  
  if (argc == 2) {
    return check_ip(argv[1],0);
  } else {
    printf("Will first try redlab, localhost, then a bogus host (bogushost)\n");
    if (try("redlab") == 0 && try("localhost") == 0 && try("bogushost") == -1) {
      printf("Ok\n");
      return 0;
    } else {
      printf("Failed\n");
      return -1;
    }

  }
}

int try(char *name) {
    struct hostent *hp;
    if (hp = gethostbyname(name)) {
      return check_ip(hp->h_addr,1);
    } else {
      printf("could not even do a gethostbyname(%s)\n",name);
      return -1;
    }
}

int check_ip(char *ip, int converted) {
  u_long addr;
  struct hostent *hp;
  char **p;
  if (!converted) {
    if ((int)(addr = inet_addr(ip)) == -1) {
      (void) printf("IP-address must be of the form a.b.c.d\n");
      return (2);
    }
  } else {
    addr = *((long *)ip);
  }
  
  hp = gethostbyaddr((char *)&addr, sizeof (addr), AF_INET);
  if (hp == NULL) {
    (void) printf("host information for %s not found\n", ip);
    return (3);
  }
  
  for (p = hp->h_addr_list; *p != 0; p++) {
    struct in_addr in;
    char **q;
    
    (void) memcpy(&in.s_addr, *p, sizeof (in.s_addr));
    (void) printf("%s\t%s", inet_ntoa(in), hp->h_name);
    for (q = hp->h_aliases; *q != 0; q++)
      (void) printf(" %s", *q);
    (void) putchar('\n');
  }
  return (0);
}

