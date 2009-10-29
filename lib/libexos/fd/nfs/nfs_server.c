
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

/*#define PRINTF_LEVEL 9 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <assert.h>

#include "fd/proc.h"
#include "nfs_rpc.h"
#include "nfs_rpc_procs.h"
#include "nfs_struct.h"
#include "nfs_xdr.h"

#include <exos/debug.h>

#include "exos/locks.h"
#include "exos/critical.h"

#include <fcntl.h>
#include "errno.h"

/* for inet_aton */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* for pripaddr */
#include <exos/netinet/in.h>

#define LOCALPORT 512 + (((envidx(__envid))*32) & (0x1ff))
extern dev_t getnewdevicenum();



static generic_server_p
get_generic_serverp(void) {
  int i;
  DPRINTF(CLUHELP_LEVEL,("get_generic_serverp:\n"));
  EnterCritical();
    for (i = 0; i < NR_NFS_SERVERS ; i++)
	if (!(FD_ISSET(i,&nfs_shared_data->inuse)))
	{
	    DPRINTF(CLUHELP_LEVEL,
		    ("found a serverp, entry[%d]\n",i));
	    FD_SET(i,&nfs_shared_data->inuse);
	    ExitCritical();
	    return(&nfs_shared_data->server[i]);
	}
    ExitCritical();
    DPRINTF(CLUHELP_LEVEL,("get_generic_serverp" 
			   "out of global server pointers!\n"));
    return (struct generic_server *) 0;
}

static void
put_generic_serverp(generic_server_p serverp) {
  int i;
  DPRINTF(CLUHELP_LEVEL,("put_generic_serverp:\n"));
  EnterCritical();
  for (i = 0; i < NR_NFS_SERVERS ; i++)
	if (FD_ISSET(i,&nfs_shared_data->inuse)) {
	  if (&nfs_shared_data->server[i] == serverp) {
	    FD_CLR(i,&nfs_shared_data->inuse);
	    ExitCritical();
	    return;
	  }
	}
    DPRINTF(CLUHELP_LEVEL,("warning, put_generic_serverp " 
			   "was given a non-inuse serverp!\n"));
    ExitCritical();
}


generic_server_p
find_generic_serverp(dev_t dev) {
  int i;
  DPRINTF(CLUHELP_LEVEL,("put_generic_serverp:\n"));
  EnterCritical();
  for (i = 0; i < NR_NFS_SERVERS ; i++)
	if (FD_ISSET(i,&nfs_shared_data->inuse)) {
	  if (nfs_shared_data->server[i].fakedevice == dev) {
	    ExitCritical();
	    return(&nfs_shared_data->server[i]);
	  }
	}
  DPRINTF(CLUHELP_LEVEL,("warning, put_generic_serverp " 
			   "was given a non-inuse serverp!\n"));
  ExitCritical();
  return (generic_server_p) 0;
}


static struct file *
get_udp_filp(int offset) {
  struct file *filp;
  struct sockaddr_in cli_addr;
  static int tries = 0; 
  extern int socketf(struct file *filp, int family, int type, int protocol);

/*
  printf(" guf %d ",offset);
*/

  if ((filp = getfilp()) == NULL) return filp;
  if (socketf(filp,PF_INET,SOCK_DGRAM,0) == -1) return NULL;

  memset((char *) &cli_addr,0,sizeof(cli_addr));
  /* in addition to clearing the structure set sin_addr to INADDR_ANY!!! */
  cli_addr.sin_family = AF_INET;
  
  for(;;) {
    cli_addr.sin_port = htons(LOCALPORT + offset + 32*tries);
    if (DOOP(filp,bind,
	     (filp,(struct sockaddr *) &cli_addr,sizeof(cli_addr))) < 0) {
      tries++;
      if (tries >= 15) goto err;
    } else {
/*
      printf("%d.",htons(cli_addr.sin_port));
*/
      break;
    }
  }
  
  if (DOOP(filp,fcntl,(filp,F_SETFL,O_NDELAY))< 0) {
    printf("error setting socket non blocking: errno %d\n",errno);
    goto err;
  }
  return filp;
err: 
  printf("get_udp_filp could not bind\n");
  putfilp(filp);
  return NULL;
  
}

static void 
put_udp_filp(struct file *filp) {
  demand(filp,bogus filp);
  if(CHECKOP(filp,close)) DOOP(filp,close,(filp));
  putfilp(filp);
}

extern int 
udp_pass_filp_ref(struct file *filp, u_int k, int envid, int ExecOnlyOrNewpid);

/* pass all filter references of an nfs server to another environment */
static void 
nfs_pass_server_ref(u_int k, int envid,  struct generic_server *serverp) {
  if (serverp->socketf) 
    udp_pass_filp_ref(serverp->socketf,k,envid,1);
}


int 
nfs_pass_all_ref(u_int k, int envid, int ExecOnlyOrNewpid) {
  int j;
  for (j = 0; j < NR_NFS_SERVERS; j++) {
    if (FD_ISSET(j,&nfs_shared_data->inuse)) {
      nfs_pass_server_ref(k,envid,&nfs_shared_data->server[j]);
    }
  }
  return 0;
}

struct generic_server *
make_server(char *host, unsigned int port) {
  char *server_ipaddr;
  struct  generic_server *server;
  struct sockaddr_in *serv_addr;

  extern void udp_use_private_buffers(void);
  extern int udp_set_nr_buffers(int nr);

  demand(host, make_server null host);
  DPRINTF(CLUHELP_LEVEL,("MAKE_SERVER: host: %s, port: %d\n",host,port));

  //kprintf("MAKE_SERVER: host: %s, port: %d  (%d)\n",host,port,__envid);
  if (*host == (char)NULL) return (NULL);

  /* get server ip addr */
  {
    struct in_addr addr;
    struct hostent *nfsserv;
    if (inet_aton(host,&addr) != 0) {
      server_ipaddr = (char *)&addr;
    } else if ((nfsserv=gethostbyname(host)) != NULL) {
      server_ipaddr = (char *)nfsserv->h_addr;
    } else {
      errno = ENOENT;		/* could not resolve name */
      return NULL;
    }
    //printf("SERVER_IPADDR %s\n",pripaddr(server_ipaddr));
  }
  
  server = get_generic_serverp();

  if (server == (struct generic_server *)0) {
    printf("get_generic_serverp failed\n");
    return server;
  }

  START(nfsc,bind);

  {
    extern void pr_filp(struct file *,char *);
    extern int udp_set_nr_buffers(int);
    assert(MAX_P_RPC >= 4);

    udp_set_nr_buffers(15);	/* this has to be done before each bind */

    if ((server->socketf = get_udp_filp(0)) == NULL) {
      put_generic_serverp(server);
      return (NULL);
    }
  }

  STOP(nfsc,bind);
  

  server->addr = &(server->hidden_addr);

  serv_addr = server->addr;

  /* zero serv_addr structure */
  memset((char *) serv_addr,0,sizeof(struct sockaddr_in));
  
  serv_addr->sin_port = htons(port);
  serv_addr->sin_family = AF_INET;
  memcpy( (char *) &serv_addr->sin_addr,server_ipaddr,4);

  /*   server->addr = serv_addr; done implicitly */
  exos_lock_init(&(server->lock));
  server->flags = 0;		/* for now */
  server->rsize = NFS_MAXRDATA;
  server->wsize = NFS_MAXWDATA;
  server->timeo = SERVERTIMEO;
  server->retrans = SERVERRETRANS;
  server->acregmin = 0;
  server->acregmax = 0;
  server->acdirmin = 0;
  server->acdirmax = 0;
  server->fakedevice = getnewdevicenum();

  /* Allocate Proxy xnode stuff */
  {
    extern void nfsc_init_dev(int);
    nfsc_init_dev(server->fakedevice);
  }

  server->nrwrites = 0;
  server->nrbcwrites = 0;
  /* pass the filter references to init process. */
  nfs_pass_server_ref(NFS_CAP,__envs[0].env_id,server);
  return(server);
}

void 
free_server(struct generic_server *serverp) {
  put_udp_filp(serverp->socketf);
  put_generic_serverp(serverp);
}

void 
change_server_port(struct generic_server *server, int port) {
  DPRINTF(CLUHELP2_LEVEL,("CHANGE_SERVER_PORT to port %d\n",port));
  server->addr->sin_port = htons(port);
}

void 
pr_server(struct generic_server *serverp) {

  printf("generic server: %p\n",serverp);
  printf("lock: %d  socketf: %p\n",serverp->lock.lock,serverp->socketf);
  printf("fakedevice: %d\n",serverp->fakedevice);
  printf("wsize: %d rsize: %d timeo: %d retrans: %d\n",
	 serverp->wsize,
	 serverp->rsize,
	 serverp->timeo,
	 serverp->retrans);
  printf("sockaddr: %p, &hidden_addr: %p\n",
	 serverp->addr, &serverp->hidden_addr);
  printf("sockaddr: family: %d, port: %d ip: %08x\n",
	 serverp->addr->sin_family,
	 ntohs(serverp->addr->sin_port),
	 ntohl(serverp->addr->sin_addr.s_addr));
  printf("number of writes: %d, number of bc aligned writes: %d\n",
	 serverp->nrwrites,serverp->nrbcwrites);

}



