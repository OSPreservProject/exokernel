
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <pwd.h>
#include <netdb.h>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <xok/env.h>
#include <xok/sysinfo.h>
#include <xok/sys_ucall.h>
#include <exos/uwk.h>
#include <exos/process.h> /* for yield */

#include <sys/stat.h>
#include <sys/wait.h>

#include <exos/netinet/in.h>
#include <exos/net/route.h>
#include <exos/net/if.h>

/* see first variables in main to see how their values are hard coded. */
/* Information it hard codes:
 ip address
 netmask
 default gateway
 interface
 ip address of NFS server
 NFS directory
 */
#define printf kprintf

#define DOMAIN "lcs.mit.edu"
struct ifinfo {
  char *interface;
  int flags;
  ipaddr_t ip;
  ipaddr_t netmask;
} ifinfo[] = {
  {"de0", IF_UP|IF_BROADCAST,{18,26,4,36},{255,255,255,0}},
  {"ed0", IF_DOWN|IF_BROADCAST,{18,26,4,33},{255,255,255,0}}
};
ipaddr_t mygateway = {18,26,4,1};
char nfs_server_ip[] = "18.26.4.40";
char nfs_server_path[] = "/disk/av0/hb/tree";

static void reapchild(int unused);
void reapchildren(void);

int
main (int argc, char **argv) {
  int status;
  int i;
  /* configure interface */

  for (i = 0 ; i < (sizeof(ifinfo)/sizeof(struct ifinfo)) ; i++) {
    struct ifinfo *ifn = &ifinfo[i];
    ipaddr_t broadcast;
    
    apply_netmask_broadcast(broadcast,(char *)&ifn->ip, (char *)&ifn->netmask);
    printf(" Name: %s ",ifn->interface);
    printf("Ip Address: %s ",pripaddr((char *)&ifn->ip));
    printf("Netmask: %s ",pripaddr((char *)&ifn->netmask));
    printf("Broadcast: %s\n",pripaddr(broadcast));
    status = ifconfig(ifn->interface,ifn->flags, 
	     (char *)&ifn->ip, (char *)&ifn->netmask,broadcast);
    printf("ifconfig %s => %d\n",ifn->interface,status);
    if (status != 0) {
      printf("ifconfig failed\n");
    }
  }
  /* add default route */
  status = route_add(ROUTE_INET,
		     ((ipaddr_t){0,0,0,0}),
		     ((ipaddr_t){0,0,0,0}),
		     mygateway);
  printf("route add default %s ==> %d\n",pripaddr(mygateway),status);

  /* start arp daemon, necessary to be started before mounting nfs server
   * so that the server can resolve our ip address, and we can resolve his.
   */
  {
    extern void arpd_main (void);
    if (fork () == 0) {
      arpd_main ();
    }
  }

  /* mount the nfs root */
  {
    extern int nfs_mount_root();
    printf("MOUNTING ROOT: ");
    status = nfs_mount_root(nfs_server_ip,nfs_server_path);
    printf("DONE\n");
    if (status != 0) {
      printf("failed (errno: %d)\n",errno);
      printf("check machine \"%s\" /etc/exports file, and make sure that you\n"
	     "either have -alldirs or the exact directory there\n"
	     "you have restarted mountd after making changes\n"
	     "and that you have the proper permission\n",nfs_server_ip);
      printf("fix the problem and try again\n");
    }
  }

  /* configuring lo0: configure loopback interface and route
   * ip packets from our machineip to our machineip via loopback
   * interface
   */
  {
    ipaddr_t lo0mask = {255,255,255,255};
	if_show();
    status = ifconfig("lo0",IF_UP|IF_LOOPBACK,
		      IPADDR_LOOPBACK, lo0mask, IPADDR_BROADCAST);
    printf("ifconfig lo0 => %d\n",status);

    for (i = 0 ; i < (sizeof(ifinfo)/sizeof(struct ifinfo)) ; i++) {
      struct ifinfo *ifn = &ifinfo[i];

      status = route_add(ROUTE_INET,
			 ifn->ip,IPADDR_BROADCAST, IPADDR_LOOPBACK);
      printf("route %s localhost ==> %d\n",pripaddr(ifn->ip),status);
    }
  }
  

  /* set the hostname */
  {
    struct hostent *h;
    if ((h = gethostbyaddr(ifinfo[0].ip,4,AF_INET)) != NULL) {
      printf("My hostname is: %s\n",h->h_name);
      sethostname(h->h_name,strlen(h->h_name));
      setdomainname(DOMAIN,strlen(DOMAIN));
    } else {
      printf("gethostbyaddr %s %d %d failed\n",pripaddr(ifinfo[0].ip),4,AF_INET);
    }
  }

  /* if there is an rc.local files execute it.  this is crucial because 
   * otherwise the machine will simple have mounted a file system and stopped
   */
#ifdef RCLOCAL
  {
    struct stat sb;
    if (stat(RCLOCAL,&sb) == 0) {
      int status;
      sys_cputs("Spawning ");
      sys_cputs(RCLOCAL);
      sys_cputs("\n");
      status = system(RCLOCAL);
      /* child */
      kprintf("%d system(%s) returned %d %d\n",getpid(),RCLOCAL,status,errno);
    }
  }
#endif /* RCLOCAL */

  /* setup reaper */
#if 1
  signal(SIGCHLD,reapchild);
  for(;;) sleep(100);
#endif

  /* the first process now sleeps forever.  It can't die, since it holds
   * references to important resources (like some memory pages)
   */
#if 0
  reapchildren();
#else
  UAREA.u_status = U_SLEEP;
  yield(-1);
#endif
  kprintf("FIRST PROCESS SHOULD NEVER PRINT THIS ==> TROUBLE\n");
  assert(0);
  return 0;
}

static void reapchild(int unused) {
  int status,pid;
  kprintf("reapchild:\n");
  while((pid = wait3(&status,WNOHANG,NULL)) > 0) {
    if (WIFEXITED(status)) 
      kprintf ("  pid %d exited with %d\n", pid,WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
      kprintf ("  pid %d signaled by %d\n", pid,WTERMSIG(status));
    else if (WIFSTOPPED(status))
      kprintf ("  pid %d stopped by %d\n", pid,WSTOPSIG(status));
    else 
      kprintf ("  BAD WAIT STATUS pid %d, status: 0x%08x errno: %d\n", pid,status,errno);
  }
}

void reapchildren(void) {
  for(;;) {
    sleep(5);
    reapchild(0);
  }
}
