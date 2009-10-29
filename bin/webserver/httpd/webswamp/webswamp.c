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
#include <string.h>
#include <assert.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>


#include <exos/netinet/hosttable.h>
#ifdef EXOPC
#include <exos/tick.h>
#include <exos/vm-layout.h>             /* for PAGESIZ */
#else
#define ae_gettick()	time(NULL)
#define ae_getrate()	1000000
#define PAGESIZ 4096
#endif

#include "web_general.h"
#include "xio/xio_tcpcommon.h"

//#define MERGE_PACKETS
#define DOPACKETCHECKS
#define BUSYWAIT
#define SERV_PORT 2001

static xio_nwinfo_t nwinfo;
static xio_dmxinfo_t dmxinfo;

static char *servername;
static map_t *serverinfo;

static int serverport = 80;
static int clientport = 80;

static int maxconns = 1;
static int numconns = 0;
static int iters = 100;
static int repeatmax = 1;                                      
static char *documentname;

static char *clientname = "cbn2";
static map_t *clientinfo;

static char reqbuf[4096];
static int reqlen = 0;

int aegis_tick_rate;

/* stucture passed for syncing up machines */ 

struct swamp_info{
  char synchost[50];
  char clientname[50];
  char servername[50];
  char documentname[50];
  int  serverport;
  int  maxconns;
  int  iters;
};

typedef struct swamp_info SWAMP_INFO;
typedef SWAMP_INFO *SWAMP_INFOPTR;
SWAMP_INFOPTR  swamp_data = NULL;


/* structure for sending the results back to sync host */
struct swamp_results{
   unsigned int filesize;
   unsigned int usecs;
   unsigned int bpms;
   unsigned int webpages;
   unsigned int inpackets;
   unsigned int outpackets;
};
typedef struct swamp_results SWAMP_RESULTS;
typedef SWAMP_RESULTS *RESULTS_PTR;
RESULTS_PTR  results_data = NULL;

/* main structure defining state of a request */

typedef struct webreq {
   struct tcb tcb;
   struct webreq *next;
   int id;
   int state;
   int command;
   int pagecnt;
   int tmpval;
   char *pathname;
   int pathnamelen;
   void *inode;
   void *waitee;
   int lastindex;
   int inbuflen;
   char *inbuf;                           
} webreq_t;

webreq_t *active_webreqs = NULL;
webreq_t *free_webreqs = NULL;              

#define WEBREQ_STATE_FREE	0x00000000
#define WEBREQ_STATE_CONNECTING	0x00000001
#define WEBREQ_STATE_SENDREQ	0x00000002
#define WEBREQ_STATE_RESPRECV	0x00000003
#define WEBREQ_STATE_CLOSING	0x00000020
#define WEBREQ_STATE_DOCLOSE	0x00000040
#define WEBREQ_STATE_DISKWAIT	0x80000000


void grab_free_webreq ()                  
{
   webreq_t *tmp = free_webreqs;
   assert (free_webreqs != NULL);
   xio_tcp_setrcvwnd (&tmp->tcb, 16384);
   numconns++;
/*                              
printf ("grab_free_webreq: tmp %p, numconns %d\n", tmp, numconns);       
*/
   free_webreqs = tmp->next;              
   tmp->next = active_webreqs;                  
   active_webreqs = tmp;                          
}


void release_webreq (webreq_t *webreq)            
{
   webreq_t *tmp = active_webreqs;        
   if (tmp == webreq) {                   
      active_webreqs = tmp->next;         
   } else {
      while ((tmp->next != NULL) && (tmp->next != webreq)) {
         tmp = tmp->next;        
      }                          
      assert (tmp->next == webreq);
      tmp->next = webreq->next;  
   }
   webreq->state = WEBREQ_STATE_FREE;
   xio_tcp_resettcb (&webreq->tcb);       

   webreq->next = free_webreqs;
   free_webreqs = webreq;
   numconns--;
/*                              
printf ("release_webreq: webreq %x, numconns %d\n", (uint) webreq, numconns);   
*/
}


webreq_t * web_server_getwebreq ()
{
   webreq_t *webreq = free_webreqs;

   /* need to deal with this case better !!! */
   assert (webreq != NULL);

   grab_free_webreq ();
//printf ("Next connection. webreq == %p, clientport %d\n", webreq, clientport);
   webreq->lastindex = 0;
   webreq->inbuflen = 0; 
   webreq->pagecnt++;
   return (webreq);
}


void webswamp_send_request (webreq_t *webreq)
{
   int sent;

   assert (((webreq->tcb.send_offset & 0x80000000) == 0) || (webreq->tcb.send_offset <= reqlen));
   webreq->tcb.snd_next -= webreq->tcb.send_offset;
   webreq->tcb.send_offset = 0;
   sent = xio_tcp_prepDataPacket (&webreq->tcb, reqbuf, reqlen, 0, 0, 0, -1);
   assert ((sent == 0) || (sent == reqlen));

   if (sent == reqlen) {
      int ret = xio_tcpcommon_sendPacket (&webreq->tcb, 0);
      assert (ret >= 0);
      webreq->state = WEBREQ_STATE_RESPRECV;
   }
}


int webswamp_gotdata (struct tcb *tcb, char *data, int len)
{
   webreq_t *webreq = (webreq_t *) tcb;
   webreq->inbuflen += len;
   return (0);
}

int
get_data_packet(int sockfd, struct sockaddr *cliaddr, int clilen)
{                                               
  int  n;
  swamp_data = malloc(sizeof(SWAMP_INFO));               // printf("Getting packet\n");

  n = recvfrom(sockfd, swamp_data, sizeof(*swamp_data), 0, cliaddr, &clilen);       
     
  servername   = swamp_data->servername;   // printf("Server:%s\n", swamp_data->servername);
  serverport   = swamp_data->serverport;   // printf("Port:%d\n", swamp_data->serverport);
  iters        = swamp_data->iters;        // printf("Iters:%d\n", swamp_data->iters);
  maxconns     = swamp_data->maxconns;     // printf("Conns:%d\n", swamp_data->maxconns);
  documentname = swamp_data->documentname; // printf("Doc:%s\n", swamp_data->documentname); 
  clientname   = swamp_data->clientname;   // printf("Client:%s\n", swamp_data->clientname);
                                           // printf("Synchost::%s\n", swamp_data->synchost);
  return(0);
}

int 
wait_for_swamp_data()
{                                              
  int fd;
  struct sockaddr_in servaddr_send, cliaddr_send;                 
    
  bzero( &servaddr_send, sizeof(servaddr_send));
  servaddr_send.sin_family = AF_INET;
  servaddr_send.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr_send.sin_port = htons(SERV_PORT);
  fd = socket(AF_INET, SOCK_DGRAM, 0);
   
  bind(fd, (struct sockaddr *)&servaddr_send,  sizeof(servaddr_send) );

  get_data_packet( fd, (struct sockaddr *)&cliaddr_send, sizeof(cliaddr_send) );  

  close(fd);  

  return(0);
}

int
send_results()
{ 
  int sockfd;                   
  struct sockaddr_in servaddr;
  struct hostent *serverent;
  //  printf("Sending Results\n");
  serverent = malloc(sizeof(struct hostent));   

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(SERV_PORT);
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  serverent = gethostbyname( swamp_data->synchost );
  bcopy( serverent->h_addr_list[0], (char *) &servaddr.sin_addr, serverent->h_length );

  sendto(sockfd, results_data, sizeof(*results_data), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
  
  close(sockfd);  
  return(0);
}

int main (int argc, char **argv)
{

   int i, j, ret, ret2;
   webreq_t *webreq;
   int repeat = 0;
   int itercount = 0;
   int filesize = 0;                                                 
   int bytes = 0;          
   int rate = ae_getrate ();                                               
   double seconds, prevseconds;
   int bpms;
   int ticks;
   int time1;                                            
   int webpages = 0;                           
   int pagebytes;
   int repeatcount = 0;
   double totaltime = 0.0;
   struct ae_recv *packet;


   if ((argc < 2) || (argc > 8)) {
      printf ("Usage: %s <servername> <docname> [ <clientname> <serverport> <maxconns> <iters> ]\n", argv[0]);
      exit (0);
   }

   if (argc >= 5 ){
      serverport = atoi (argv[4]);
      if ((serverport < 0) || (serverport > 65535)) {
         printf ("Invalid server port number specified: %d\n", serverport);
         exit (0);
      }
   }

   if (argc >= 6 ) {
      maxconns = atoi (argv[5]);
      if ((maxconns < 0) || (maxconns > 128)) {
         printf ("Invalid maximum concurrent connection count specified: %d\n", maxconns);
         exit (0);
      }
   }

   if (argc >= 7){
      iters = atoi (argv[6]);
      if (iters < 0) {
         printf ("Invalid maximum iteration count specified: %d\n", iters);
         exit (0);
      }
   }


 if(strcmp(argv[1], "-wait")){
   servername = argv[1];
   serverinfo = hosttable_map;
   while (serverinfo->name[0] != (char)0) {
      if (!strcasecmp(servername, serverinfo->name)) {
         break;
      }
      serverinfo++;
   }
   if ((serverinfo == NULL) || (serverinfo->name[0] == (char)0)) {
      printf ("Server name unknown: %s\n", servername);
      exit (0);
   }

   if (argc >= 4) {
      clientname = argv[3];
   }
   clientinfo = hosttable_map;
   while (clientinfo->name[0] != (char)0) {
      if (!strcasecmp(clientname, clientinfo->name)) {
         break;
      }
      clientinfo++;
   }
   if ((clientinfo == NULL) || (clientinfo->name[0] == (char)0)) {
      printf ("Client name unknown: %s\n", clientname);
      exit (0);
   }

   documentname = argv[2];
   sprintf (reqbuf, "GET /%s HTTP/1.0\r\n\r\n", documentname);
   reqlen = strlen (reqbuf);
   printf ("Request string (%d): %s", reqlen, reqbuf);

   aegis_tick_rate = ae_getrate();

   printf ("Going to swamp %s (%x) from %s (%x)\n", servername, serverinfo->eth_addr[5], clientname, clientinfo->eth_addr[5]);

   clientport = (getpid() & 0xF) << 12;
   printf ("webswamp: pid %d, first TCP port %d\n", getpid(), clientport);

   if (clientport == 0) {
      printf ("PID-based initial port selection chose 0, re-run process\n");
      exit (0);
   } 
 }

 wait: 
   if( !strcmp(argv[1], "-wait") ){
     itercount = 0;
     filesize = 0;
     webpages = 0;
     inpackets = 0;
     outpackets = 0;    
     
     wait_for_swamp_data();

     if( !strcmp(documentname , "shutdown")){
       printf("Recieved the shutdown signal...exiting!!!\n");
       exit(0);
     }

     serverinfo = hosttable_map;
     while (serverinfo->name[0] != (char)0) {
      if (!strcasecmp(servername, serverinfo->name)) {
         break;
      }
     serverinfo++;
     } 
      if ((serverinfo == NULL) || (serverinfo->name[0] == (char)0)) {
       printf ("Server name unknown: %s\n", servername);
       exit (0);
     } 
     
     clientinfo = hosttable_map;
     while (clientinfo->name[0] != (char)0) {
      if (!strcasecmp(clientname, clientinfo->name)) {
         break;
      }
       clientinfo++;
      }
      if ((clientinfo == NULL) || (clientinfo->name[0] == (char)0)) {
      printf ("Client name unknown: %s\n", clientname);
      exit (0);
      }

     sprintf (reqbuf, "GET /%s HTTP/1.0\r\n\r\n", documentname);  
     reqlen = strlen (reqbuf);
     printf ("Request string (%d): %s", reqlen, reqbuf);

     aegis_tick_rate = ae_getrate();

     printf ("Going to swamp %s (%x) from %s (%x)\n", servername, serverinfo->eth_addr[5], clientname, 
	     clientinfo->eth_addr[5]);

     if(repeat == 0)
       clientport = (getpid() & 0xF) << 12;
     else
       clientport++;

     printf ("webswamp: pid %d, first TCP port %d\n", getpid(), clientport);
   }


 
  if(repeat == 0){
   xio_tcp_demux_init (&dmxinfo, 0);
   xio_net_wrap_init (&nwinfo, malloc(32 * PAGESIZ), (32*PAGESIZ)); 
   ret = xio_net_wrap_getdpf_tcp (&nwinfo, -1, serverport);
   assert (ret != -1);   
  }

   for (i=0; i<numblocks(maxconns,(PAGESIZ/sizeof(webreq_t))); i++) {
      webreq = (webreq_t *) malloc(PAGESIZ);
      assert (webreq);
      bzero ((char *) webreq, PAGESIZ);
      for (j=0; j<(PAGESIZ/sizeof(webreq_t)); j++) {
         webreq[j].id = (i * (PAGESIZ/sizeof(webreq_t))) + j;
         webreq[j].state = WEBREQ_STATE_FREE;
         webreq[j].pathname = NULL;
         webreq[j].inbuf = NULL;
         webreq[j].waitee = NULL;
         xio_tcp_inittcb (&webreq[j].tcb);
         webreq[j].next = free_webreqs;
         free_webreqs = &webreq[j];
      }                                                  
   }

   prevseconds = (double) ae_gettick() * (double) rate / (double) 1000000.0;

repeat:
   filesize = 0;
   bytes = 0;

   time1 = ae_gettick();

   for (i=0; i<maxconns; i++) {
      webreq = web_server_getwebreq();
      webreq->state = WEBREQ_STATE_CONNECTING;
      printf ("Opening connection with clientport %d\n", clientport);
      webreq->tcb.netcardno = xio_net_wrap_getnetcardno (&nwinfo, clientinfo->eth_addr);
      assert (webreq->tcb.netcardno >= 0);
      xio_tcp_bindsrc (&webreq->tcb, (clientport & 0xFFFF), clientinfo->ip_addr, clientinfo->eth_addr);
      xio_tcp_initiate_connect (&webreq->tcb, (clientport & 0xFFFF), serverport, serverinfo->ip_addr, serverinfo->eth_addr);
      ret = xio_tcpcommon_sendPacket (&webreq->tcb, 0);
      assert (ret >= 0);
      ret = xio_tcp_demux_addconn (&dmxinfo, &webreq->tcb, 1);
      assert (ret != -1);
      clientport++;
      itercount++;
   }

printf ("Entering main loop\n");

   while (numconns > 0) { 

      seconds = (double) ae_gettick() * (double) rate / (double) 1000000.0;
      if ((int)seconds > (int)prevseconds) {
         webreq_t *next = active_webreqs;
         while ((webreq = next)) {
            next = webreq->next;
            if (!xio_tcp_timedout(&webreq->tcb)) {
               continue;
            }
            ret = xio_tcp_timeout (&webreq->tcb);
//if (webreq->tcb.send_ready) {
//printf ("retransmit: flags %x\n", webreq->tcb.send_ready);
//}
            ret2 = xio_tcpcommon_sendPacket (&webreq->tcb, 0);
            assert (ret2 >= 0);
            if (ret == 0) {
               continue;
            }
            if (xio_tcp_closed (&webreq->tcb)) {
               printf ("Connection was RST by timeout (webreq %p, tcpsrc %d, tcpdst %d)\n", webreq, (uint)ntohs(webreq->tcb.tcpsrc), (uint)ntohs(webreq->tcb.tcpdst));
               xio_tcp_demux_removeconn (&dmxinfo, &webreq->tcb);
               release_webreq (webreq);
            }
            if ((webreq->state == WEBREQ_STATE_RESPRECV) && (webreq->tcb.send_offset < reqlen)) {
               webswamp_send_request (webreq);
            }
         }
         prevseconds = seconds;
      }

#ifndef BUSYWAIT
      if (1) {
                /* GROK -- this should be the tcb with the shortest timeout */
         struct tcb *tcb = (active_webreqs) ? &active_webreqs->tcb : NULL;
                /* GROK - busywait for a short time if packet are expected  */
                /*        soon (too avoid cswitching back and forth).       */
         xio_tcp_waitforChange (&nwinfo, tcb);
      }
#endif

      webreq = active_webreqs;
      while (webreq) {
         if ((webreq->state == WEBREQ_STATE_RESPRECV) && (webreq->tcb.send_offset < reqlen)) {
            webswamp_send_request (webreq);
         }
         if (xio_tcp_closed(&webreq->tcb)) {
            webreq_t *tmp = webreq;
            webreq = webreq->next;
            printf ("connection timed out (webreq %p, srcport %d, dstport %d\n", tmp, (uint)ntohs(tmp->tcb.tcpsrc), (uint)ntohs(tmp->tcb.tcpdst));
            xio_tcp_demux_removeconn (&dmxinfo, &tmp->tcb);
            release_webreq (tmp);
            continue;
         }
         webreq = webreq->next;
      }

      while ((packet = xio_net_wrap_getPacket(&nwinfo)) != NULL) {
         webreq = (webreq_t *) xio_tcp_demux_searchconns (&dmxinfo, packet->r[0].data);
//printf ("got packet: len %d, webreq %p, state %d, tstate %d\n", packet->r[0].sz, webreq, ((webreq) ? webreq->state : -1), ((webreq) ? webreq->tcb.state : -1));
         xio_tcp_clobbercksum (packet->r[0].data, packet->r[0].sz);
#ifdef DOPACKETCHECKS
         if (!xio_tcp_packetcheck (packet->r[0].data, packet->r[0].sz, NULL)) {
            kprintf ("tcp packetcheck failed -- toss it\n");
            xio_net_wrap_returnPacket (&nwinfo, packet);
         } else
#endif
                if (webreq) {
            xio_tcp_handlePacket (&webreq->tcb, packet->r[0].data, 0);
            if (webreq->tcb.indata) {
               int livedatalen = xio_tcp_getLiveInDataLen (&webreq->tcb);
               char *livedata = xio_tcp_getLiveInData (&webreq->tcb);
               webswamp_gotdata (&webreq->tcb, livedata, livedatalen);
            }
            xio_net_wrap_returnPacket (&nwinfo, packet);
            if (webreq->tcb.send_ready) {
#ifdef MERGE_PACKETS
               if ((webreq->state != WEBREQ_STATE_CONNECTING) && (webreq->tcb.state != TCB_ST_CLOSE_WAIT)) {
#else
               if (1) {
#endif
                  int ret = xio_tcpcommon_sendPacket (&webreq->tcb, 0);
                  assert (ret >= 0);
               }
               webreq->tcb.send_ready = 0;
            }

            if ((webreq->state == WEBREQ_STATE_CONNECTING) && (xio_tcp_writeable (&webreq->tcb))) {
               webreq->state = WEBREQ_STATE_SENDREQ;
            }

            if (webreq->state == WEBREQ_STATE_SENDREQ) {
		/* webreq_send_request advances state if appropriate */
               webswamp_send_request (webreq);
            }
        
            if (webreq->state == WEBREQ_STATE_RESPRECV) {
		/* Wait until no more data to be read (data is acquired via */
		/* gotdata(). */
               if (xio_tcp_readable (&webreq->tcb) == 0) {
                  filesize += webreq->inbuflen;
                  bytes += webreq->inbuflen;
                  pagebytes += webreq->inbuflen;
                  xio_tcp_initiate_close (&webreq->tcb);
		   /* GROK -- does DOCLOSE server a purpose any more?? */
                  ret = 1;
                  if (ret == 0) {
                     webreq->state = WEBREQ_STATE_DOCLOSE;
                  } else {
                     webreq->state = WEBREQ_STATE_CLOSING;
                  }
                  ret2 = xio_tcpcommon_sendPacket (&webreq->tcb, 0);
                  assert (ret2 >= 0);
               }
            }

            if (webreq->state == WEBREQ_STATE_DOCLOSE) {
               int ret = 1;
               xio_tcp_initiate_close (&webreq->tcb);
               if (ret != 0) {
                  webreq->state = WEBREQ_STATE_CLOSING;
               }
               ret2 = xio_tcpcommon_sendPacket (&webreq->tcb, 0);
               assert (ret2 >= 0);
            }
	    //printf ("got packet: len %d, webreq %p, state %d, tstate %d\n", packet->r[0].sz, webreq, webreq->state, webreq->tcb.state);

            if (xio_tcp_closed(&webreq->tcb)) {
               webreq_t *tmp = webreq;
               webreq = webreq->next;
               if (tmp->state != WEBREQ_STATE_CLOSING) {
                  printf ("Connection was RST (webreq %p, srcport %d, dstport %d\n", tmp, (uint)ntohs(tmp->tcb.tcpsrc), (uint)ntohs(tmp->tcb.tcpdst));
               }
               xio_tcp_demux_removeconn (&dmxinfo, &tmp->tcb);
               release_webreq (tmp);
               webpages++;
               if (itercount < iters) {
                  tmp = web_server_getwebreq ();
                  tmp->state = WEBREQ_STATE_CONNECTING;
                  xio_tcp_bindsrc (&tmp->tcb, (clientport & 0xFFFF), clientinfo->ip_addr, clientinfo->eth_addr);
                  xio_tcp_initiate_connect (&tmp->tcb, (clientport & 0xFFFF), serverport, serverinfo->ip_addr, serverinfo->eth_addr);
                  ret2 = xio_tcpcommon_sendPacket (&tmp->tcb, 0);
                  assert (ret2 >= 0);
                  ret = xio_tcp_demux_addconn (&dmxinfo, &tmp->tcb, 1);
                  assert (ret != -1);
                  clientport++;
                  itercount++;
               }
               continue;
            }

            webreq = webreq->next;
         } else {
            xio_tcpcommon_handleRandPacket (packet->r[0].data, &nwinfo);
            xio_net_wrap_returnPacket (&nwinfo, packet);
         }
      }
   }

   assert (rate > 0);
   ticks = ae_gettick() - time1;                                                
   bpms = (ticks > 0) ? ((filesize / (ticks * rate / 1000)) * 1000) : -1;
   totaltime += (double) ticks * (double) rate / 1000000.0;
   printf ("bytes: %7d, msecs %8d, bytes/sec: %6d, pages/sec %f\n", filesize, (ticks * rate / 1000), bpms, ((totaltime > 0.0) ? ((double)webpages/totaltime) : -1.0));

   repeatcount++;
   if (repeatcount < repeatmax) {
      goto repeat;
   }
   {extern int inpackets, outpackets;
   printf ("Done (webpages %d, inpackets %d, outpackets %d)\n", webpages, inpackets, outpackets);
   }
   webreq = free_webreqs;

/*
   while (webreq) {
      printf ("conn %d did %d requests\n", webreq->id, webreq->pagecnt);
      webreq = webreq->next;
   }
*/

   if( !strcmp(argv[1], "-wait") ){
      results_data = malloc(sizeof(SWAMP_RESULTS));

      results_data->filesize   = htonl(filesize); 
      results_data->usecs      = htonl((ticks * rate/ 1000));
      results_data->bpms       = htonl(bpms);
      results_data->webpages   = htonl(webpages);
      results_data->inpackets  = htonl(inpackets);
      results_data->outpackets = htonl(outpackets);
 
      send_results();

      repeat = 1;
      totaltime = 0;
      goto wait;
   }

   exit(0);
}

