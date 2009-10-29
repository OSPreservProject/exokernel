
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

#include <netinet/hosttable.h>
#include <sys/tick.h>

#include <exos/vm-layout.h>             /* for PAGESIZ */

#include <debug.h>

#include "web_general.h"
#include "fast_alloc.h"
#include "xio/xio_tcp.h"

static char *servername;
static map_t *serverinfo;

static int serverport = 80;
static int clientport = 0;

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

/* main structure defining state of a request */

#define WEB_REQT_SIZE           256
#define WEB_REQT_SPACESIZE      (WEB_REQT_SIZE - (6*sizeof(int)) - (5*sizeof(void *)) - sizeof(struct tcb))
#define WEB_REQT_INBUFSIZE      1460

typedef struct webreq {
   struct tcb tcb;
   struct webreq *next;
   int state;
   int command;
   int tmpval;
   char *pathname;
   int pathnamelen;
   void *inode;
   void *waitee;
   int lastindex;
   int inbuflen;
   char *inbuf;                           
   char space[WEB_REQT_SPACESIZE];
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
   numconns++;
/*                              
printf ("grab_free_webreq: tmp %x, numconns %d\n", (uint) tmp, numconns);       
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
   web_tcp_resettcb (&webreq->tcb);       

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
DPRINTF (2, ("Next connection. webreq == %p, clientport %d\n", webreq, clientport));
   webreq->lastindex = 0;
   webreq->inbuflen = 0; 
   return (webreq);
}


void webswamp_send_request (webreq_t *webreq)
{
   int more = 0;
   int sent;

   sent = web_tcp_senddata (&webreq->tcb, reqbuf, reqlen, 0, 0, 0, &more, NULL);
   assert ((sent == 0) || (sent == reqlen));

   if (sent == reqlen) {
      webreq->state = WEBREQ_STATE_RESPRECV;
   }
}


void webdemo_main (int argc, char **argv)
{
   int i, j, ret;
   webreq_t *webreq;
   int itercount = 0;
   int filesize = 0;                                                 
   int bytes = 0;          
   int rate = ae_getrate ();                                               
   int bpms;
   int ticks;
   int time1;                                            
   int webpages = 0;                           
   int pagebytes;
   int repeatcount = 0;
   double totaltime = 0.0;                                               

   if ((argc < 3) || (argc > 7)) {
      printf ("Usage: %s <servername> <docname> [ <clientname> <serverport> <maxconns> <iters> ]\n", argv[0]);
      exit (0);
   }

   if (argc >= 5) {
      serverport = atoi (argv[4]);
      if ((serverport < 0) || (serverport > 65535)) {
         printf ("Invalid server port number specified: %d\n", serverport);
         exit (0);
      }
   }

   if (argc >= 6) {
      maxconns = atoi (argv[5]);
      if ((maxconns < 0) || (maxconns > 128)) {
         printf ("Invalid maximum concurrent connection count specified: %d\n", maxconns);
         exit (0);
      }
   }

   if (argc >= 7) {
      iters = atoi (argv[6]);
      if (iters < 0) {
         printf ("Invalid maximum iteration count specified: %d\n", iters);
         exit (0);
      }
   }

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

   if (clientport == 0) {
      clientport = (getpid() & 0xF) << 12;
      printf ("webswamp: pid %d, first TCP port %d\n", getpid(), clientport);

      if (clientport == 0) {
         printf ("PID-based initial port selection chose 0, re-run process\n");
         exit (0);
      }
   }

   web_tcp_getmainport (serverport, NULL, 1);
/*
*/
{extern int ash_ganger_info[];
extern int webtcp_demuxid;
printf ("setting ash_ganger_info %p for fid %d to print\n", ash_ganger_info, webtcp_demuxid);
ash_ganger_info[webtcp_demuxid] = 1;
}

   for (i=0; i<numblocks(maxconns,(PAGESIZ/WEB_REQT_SIZE)); i++) {
      webreq = (webreq_t *) alloc_phys_page();
      assert (webreq);
      for (j=0; j<(PAGESIZ/WEB_REQT_SIZE); j++) {
         webreq[j].state = WEBREQ_STATE_FREE;
         webreq[j].pathname = NULL;
         webreq[j].inbuf = NULL;
         webreq[j].waitee = NULL;
         web_tcp_inittcb (&webreq[j].tcb, webreq[j].space, WEB_REQT_SPACESIZE);
         webreq[j].next = free_webreqs;
         free_webreqs = &webreq[j];
      }                                                  
   }

repeat:
   filesize = 0;
   bytes = 0;

   time1 = ae_gettick();

   for (i=0; i<maxconns; i++) {
      webreq = web_server_getwebreq();
      webreq->state = WEBREQ_STATE_CONNECTING;
      printf ("Opening connection with clientport %d\n", clientport);
      ret = web_tcp_initiate_connect (&webreq->tcb, (clientport & 0xFFFF), serverinfo, clientinfo);
      assert (ret);
      clientport++;
      itercount++;
   }

   while (numconns > 0) {

#if 0
      webreq = active_webreqs;
      while (webreq) {
#else
      while ((webreq = (webreq_t *) xio_tcp_handlePacket (xio_tcp_getPacket())) != NULL) {
#endif

         if ((webreq->state == WEBREQ_STATE_CONNECTING) && (web_tcp_writable (&webreq->tcb))) {
            webreq->state = WEBREQ_STATE_SENDREQ;
         }

         if (webreq->state == WEBREQ_STATE_SENDREQ) {
		/* webreq_send_request advances state if appropriate */
            webswamp_send_request (webreq);
         }
        
         if (webreq->state == WEBREQ_STATE_RESPRECV) {
            int len = web_tcp_getdata (&webreq->tcb, &webreq->inbuf);
            if (len > webreq->inbuflen) {
               webreq->inbuflen = len;
            }
            if (web_tcp_readable (&webreq->tcb) == 0) {
               len = web_tcp_getdata (&webreq->tcb, &webreq->inbuf);
               if (len > webreq->inbuflen) {
                  webreq->inbuflen = len;
               }
               filesize += webreq->inbuflen;
               bytes += webreq->inbuflen;
               pagebytes += webreq->inbuflen;
               ret = web_tcp_initiate_close (&webreq->tcb);
               if (ret == 0) {
                  webreq->state = WEBREQ_STATE_DOCLOSE;
               } else {
                  webreq->state = WEBREQ_STATE_CLOSING;
               }
            }
         }

         if (webreq->state == WEBREQ_STATE_DOCLOSE) {
            if (web_tcp_initiate_close (&webreq->tcb) != 0) {
               webreq->state = WEBREQ_STATE_CLOSING;
            }
         }

         if ((webreq->state == WEBREQ_STATE_CLOSING) && (web_tcp_closed (&webreq->tcb))) {
            webreq_t *tmp = webreq;
            webreq = webreq->next;
            release_webreq (tmp);
            webpages++;
            if (itercount < iters) {
               tmp = web_server_getwebreq ();
               tmp->state = WEBREQ_STATE_CONNECTING;
               ret = web_tcp_initiate_connect (&tmp->tcb, (clientport & 0xFFFF), serverinfo, clientinfo);
               assert (ret);
               clientport++;
               itercount++;
            }
            continue;
         }

         webreq = webreq->next;
      }
   }

   ticks = ae_gettick() - time1;                                                
   bpms = (filesize / (ticks * rate / 1000)) * 1000;
   totaltime += (double) ticks * (double) rate / 1000000.0;
   printf ("bytes: %7d, usecs %8d, bytes/sec: %6d, pages/sec %f\n", filesize, (ticks * rate / 1000), bpms, ((double)webpages/totaltime));

   repeatcount++;
   if (repeatcount < repeatmax) {
      goto repeat;
   }

   printf ("Done\n");
}

