#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <pwd.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <xok/env.h>
#include <xok/sysinfo.h>
#include <exos/uwk.h>

#include <sys/stat.h>

#include <exos/netinet/in.h>
#include <exos/net/route.h>
#include <exos/net/if.h>

#include <exos/netinet/hosttable.h>
#include <exos/site-conf.h>


char sname[] = MY_NFS_ROOT_HOST;
char dname[] = MY_NFS_ROOT_PATH;

void get_nfs_root (void) {
  int status;

  {
    extern int nfs_mount_root();
    char ipname[16];
    struct in_addr addr;
    struct hostent *nfsserv;
    if (inet_aton(sname,&addr) != 0) {
      /* server name is in IP address form */
      strcpy(ipname,sname);
    } else if ((nfsserv=gethostbyname_hosttable(sname)) != NULL) {
      strcpy(ipname,pripaddr((char *)nfsserv->h_addr));
    } else {
      printf("rconsoled (setup_net.c): DNS resolve failed for nfs server %s\n",
	     sname);
      printf("please use ip address of server as your NFS_ROOT_HOST variable or\n "
	     "make sure your /etc/resolv.conf is configured correctly\n");
      exit(-1);
    }
    
    printf("MOUNTING ROOT (%s:%s)\n",ipname,dname);
    status = nfs_mount_root(ipname,dname);
    printf("Done mounting root\n");
    if (status != 0) {
      printf("failed (errno: %d)\n",errno);
      printf("check machine \"%s (%s)\" /etc/exports file, and make sure that you\n"
	     "either have -alldirs or the exact directory there\n"
	     "you have restarted mountd after making changes\n"
	     "and that you have the proper permission\n",sname,ipname);
      printf("fix the problem and try again\n");
    }
  }

}
