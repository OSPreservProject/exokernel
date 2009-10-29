
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
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <ctype.h>
#include <sys/param.h>

#ifdef EXOPC
#include <exos/vm-layout.h>             /* for PAGESIZ */
#include <fd/proc.h>
#include <exos/tick.h>
#else
#define StaticAssert	assert
#define PAGESIZ 4096
#define min(a,b)	(((a) <= (b)) ? (a) : (b))
#endif

#if 1
#define DPRINTF(a,b)    if ((a) < 0) { printf b; }
#else
#define DPRINTF(a,b)
#endif

#include "web_general.h"
#include "web_config.h"
#include "web_alias.h"
#include "web_header.h"
#include "web_error.h"
#include "web_datetime.h"

#ifndef HIGHLEVEL
#include "name_cache.h"
#include "xio/xio_tcpcommon.h"
#include <exos/netinet/cksum.h>
#include "web_fs.h"
#if 0
#include <sys/pctr.h>
#endif
#else
#include "sock_tcp.h"
#endif

/*
#define USEINPACKETS
#define BUSYWAIT
*/
#define PRECOMPUTE_CHECKSUMS
#define MERGE_CACHERETRANS
#define MERGE_PACKETS

#define WEB_MAX_INPUTMIMEHEADERS	64

static int numconns = 0;

#ifndef HIGHLEVEL
static nc_t *namecache;
dinode_t *DocumentRootInode = NULL;
int DocumentRootInodeNum = -1;
static xio_dmxinfo_t dmxinfo;
static xio_twinfo_t *twinfo = NULL;
static xio_nwinfo_t nwinfo;
#else
static int mainfd = -1;
static int connfds[WEB_MAX_CONNS];
static int writefds[WEB_MAX_CONNS];
#endif

/* main structure defining state of a request */

#ifdef USEINPACKETS
#define WEB_REQT_SIZE		512
#define WEB_REQT_INBUFSIZE	4
#else
#define WEB_REQT_SIZE		2048
#define WEB_REQT_INBUFSIZE	1460
#endif

/* how often, in usecs, to check for timed-out packets */
#define TIMEOUT_POLL_INTERVAL 100000 

typedef struct webreq {
#ifndef HIGHLEVEL
   struct tcb tcb;
#else
   int sockfd;
#endif
   int webreqno;
   struct webreq *next;
   int state;
   int command;
   int tmpval;
   char *pathname;
   int pathnamelen;
#ifndef HIGHLEVEL
#ifndef MERGE_CACHERETRANS
   int docloseafterwrites;
   int write_offset;
#endif
   dinode_t *inode;
   buffer_t *waitee;
#else
   int inode;
#endif
   int lastindex;
   int max_send_offset;
   int recompute_checksums;
   int inbuflen;
   char *inbuf;
   void *inpacket;
#ifndef USEINPACKETS
   char inbufspace[WEB_REQT_INBUFSIZE];
#endif
} webreq_t;

static webreq_t *active_webreqs = NULL;
static webreq_t *free_webreqs = NULL;

#define WEBREQ_STATE_FREE	0x00000000
#define WEBREQ_STATE_REQRECV	0x00000007
#define WEBREQ_STATE_REQRECV_COMMAND	0x00000001
#define WEBREQ_STATE_REQRECV_PATHNAME	0x00000002
#define WEBREQ_STATE_REQRECV_DOCARGS	0x00000003
#define WEBREQ_STATE_REQRECV_PROTOCOL	0x00000004
#define WEBREQ_STATE_REQRECV_MIMEHEADER	0x00000005
#define WEBREQ_STATE_FINDDOC	0x00000008
#define WEBREQ_STATE_SENDDOC	0x00000010
#define WEBREQ_STATE_CLOSING	0x00000020
#define WEBREQ_STATE_DISKWAIT	0x80000000

#define WEBREQ_TYPE_UNKNOWN	0
#define WEBREQ_TYPE_GET		1
#define WEBREQ_TYPE_HEAD	2
#define WEBREQ_TYPE_POST	3
#define WEBREQ_TYPE_OLDGET	4


static webreq_t * webreq_alloc ()
{
   webreq_t *webreq = free_webreqs;
   assert (free_webreqs != NULL);
   free_webreqs = webreq->next;
   webreq->next = active_webreqs;
   active_webreqs = webreq;

   webreq->state = WEBREQ_STATE_REQRECV_COMMAND;
   webreq->lastindex = 0;
   webreq->inbuflen = 0;
   webreq->inbuf = NULL;
   webreq->inpacket = NULL;
   webreq->recompute_checksums = 0;
   webreq->pathname = NULL;

   numconns++;
/*
printf ("webreq_alloc: tmp %p, numconns %d\n", webreq, numconns);
*/
   return (webreq);
}


static void webreq_free (webreq_t *webreq)
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

#ifndef HIGHLEVEL

#ifndef MERGE_CACHERETRANS
   if (&webreq->tcb.inbuffers) {
      xio_tcpbuffer_reclaimInBuffers (&webreq->tcb);
   }
   if (&webreq->tcb.outbuffers) {
      xio_tcpbuffer_reclaimOutBuffers (&webreq->tcb);
   }
   webreq->docloseafterwrites = 0;
   webreq->write_offset = 0;
#endif

   xio_tcp_resettcb (&webreq->tcb);
   webreq->tcb.rcv_wnd = 1460;

   if (webreq->inode != NULL) {
      web_fs_releaseDInode (webreq->inode, alfs_FSdev, 0);
      webreq->inode = NULL;
   }

   if (webreq->inpacket != NULL) {
      xio_net_wrap_returnPacket (&nwinfo, webreq->inpacket);
   }
#endif

   webreq->next = free_webreqs;
   free_webreqs = webreq;
   numconns--;
/*
printf ("webreq_free: webreq %x, numconns %d\n", (uint) webreq, numconns);
*/
}


#define WEB_MAX_COMMANDLEN	4
int translate_command (char *command)
{
   int ret = WEBREQ_TYPE_UNKNOWN;
   int i;

   for (i=0; i<WEB_MAX_COMMANDLEN; i++) {
      if (command[i] == 0) {
         break;
      } else if (islower(command[i])) {
         command[i] = toupper(command[i]);
      }
   }

   if ((bcmp (command, "GET", 3) == 0) && (command[3] == (char)0)) {
      ret = WEBREQ_TYPE_GET;
   } else if ((bcmp (command, "HEAD", 4) == 0) && (command[4] == (char)0)) {
      ret = WEBREQ_TYPE_HEAD;
   } else if ((bcmp (command, "POST", 4) == 0) && (command[4] == (char)0)) {
      //ret = WEBREQ_TYPE_POST;
   }

   return (ret);
}
	

void translate_mimeheader (webreq_t *webreq, char *buf)
{
}


void bad_request (webreq_t *webreq, int errorval)
{
   char *buf = webreq->pathname;
   int buflen = WEB_REQT_INBUFSIZE - (buf - webreq->inbuf);
   int headerlen = 0;

   if (buf == NULL) {
      assert ((errorval != WEBREQ_ERROR_POORDIRNAME) && (errorval != WEBREQ_ERROR_MOVEDTEMP));
      webreq->pathnamelen = 0;
      buf = &webreq->inbufspace[0];
      webreq->pathname = buf;
   }
   buf = (char *) ((uint)(buf + webreq->pathnamelen + 1 + 3) & 0xFFFFFFFC);

   if (webreq->command != WEBREQ_TYPE_OLDGET) {
      headerlen += web_error_header (buf, buflen, errorval);
      headerlen += web_header_date ((buf + headerlen), (buflen - headerlen));
      headerlen += web_header_server ((buf + headerlen), (buflen - headerlen));
      if ((errorval == WEBREQ_ERROR_POORDIRNAME) || (errorval == WEBREQ_ERROR_MOVEDTEMP)) {
         char tmpsave = webreq->pathname[(webreq->pathnamelen+1)];
         if (errorval == WEBREQ_ERROR_POORDIRNAME) {
            webreq->pathname[webreq->pathnamelen] = '/';
            webreq->pathname[(webreq->pathnamelen+1)] = (char) NULL;
         }
         headerlen += web_header_location ((buf + headerlen), (buflen - headerlen), webreq->pathname, 1);
         webreq->pathname[webreq->pathnamelen] = (char) NULL;
         webreq->pathname[(webreq->pathnamelen+1)] = tmpsave;
      }
      headerlen += web_header_contenttype ((buf + headerlen), (buflen - headerlen), mime_contenttype_html);
      headerlen += web_header_contentlength ((buf + headerlen), (buflen - headerlen), web_error_messagelen (errorval, webreq->pathname));
      headerlen += web_header_end ((buf + headerlen), (buflen - headerlen));
   }
   headerlen += web_error_message ((buf + headerlen), (buflen - headerlen), errorval, webreq->pathname);
   if (headerlen >= buflen) {
      printf ("Can't fit header and pathname into webreq->inbuf (%d >= %d) \n", headerlen, buflen);
      exit (0);
   }
   DPRINTF (4, ("header offset %d, header length %d, space left %d\n", (buf - webreq->inbuf), headerlen, (buflen - headerlen)));
   webreq->tmpval = headerlen;
   webreq->command = WEBREQ_TYPE_HEAD;
printf ("why in bad request??\n");
   webreq->state = WEBREQ_STATE_SENDDOC;
#ifndef HIGHLEVEL
   webreq->waitee = NULL;
#endif
}


void web_construct_header (webreq_t *webreq)
{
   char *buf = (char *) ((uint)(webreq->pathname + webreq->pathnamelen + 1 + 3) & 0xFFFFFFFC);
   int buflen = WEB_REQT_INBUFSIZE - (buf - webreq->inbuf);
   int headerlen = 0;
#ifdef HIGHLEVEL
   struct stat statbuf;
   int ret = fstat (webreq->inode, &statbuf);
   assert (ret == 0);
#endif

   assert (webreq->pathname);

   if (webreq->command == WEBREQ_TYPE_OLDGET) {
      webreq->tmpval = 0;
      return;
   }
   headerlen += web_header_goodstatus (buf, buflen);
   headerlen += web_header_date ((buf + headerlen), (buflen - headerlen));
   headerlen += web_header_server ((buf + headerlen), (buflen - headerlen));

#ifndef HIGHLEVEL
   headerlen += web_header_contenttype ((buf + headerlen), (buflen - headerlen), ((webreq->inode->type >> 16) - 1));
   headerlen += web_header_lastmodified ((buf + headerlen), (buflen - headerlen), webreq->inode->modTime);
   headerlen += web_header_contentlength ((buf + headerlen), (buflen - headerlen), webreq->inode->length);
   /* if inode->contentencoding, then add said mime info */
   if (webreq->inode->type & 0x0000FF00) {
      headerlen += web_header_contentencoding ((buf + headerlen), (buflen - headerlen), (((webreq->inode->type & 0x0000FF00) >> 8)) - 1);
   }

#else
	/* GROK -- should really do a name based evaluation here!!! */
   headerlen += web_header_contenttype ((buf + headerlen), (buflen - headerlen), 0);
   headerlen += web_header_lastmodified ((buf + headerlen), (buflen - headerlen), statbuf.st_mtime);
   headerlen += web_header_contentlength ((buf + headerlen), (buflen - headerlen), (int)statbuf.st_size);
	/* GROK -- need to do the name-based contentencoding too!! */
#endif

   headerlen += web_header_end ((buf + headerlen), (buflen - headerlen));
   if (headerlen >= buflen) {
      printf ("Can't fit header and pathname into webreq->inbuf (%d >= %d)\n", headerlen, buflen);
      exit (0);
   }
   DPRINTF (2, ("header offset %d, header length %d, space left %d\n", (buf - webreq->inbuf), headerlen, (buflen - headerlen)));
   webreq->tmpval = headerlen;
}


#ifndef HIGHLEVEL

int web_server_nameToInum (webreq_t *webreq, int currdir, char *name, int namelen)
{
   int nextdir;
   dinode_t *inode;
/*
   printf ("checking name cache: root %d, name %s, len %d\n", currdir, name, namelen);
*/
   if (name_cache_findEntry(namecache, currdir, name, namelen, &nextdir) == 0) {
      nextdir = web_alias_match (currdir, name, namelen);
      if (nextdir == -1) {
         inode = web_fs_getDInode (currdir, alfs_FSdev, (BUFFER_READ|BUFFER_ASYNC));
         if (inode == NULL) {
            nextdir = 0;
            goto nameToInum_done;
         }
         nextdir = web_fs_lookupname (alfs_FSdev, inode, name, namelen, (BUFFER_READ|BUFFER_ASYNC));
         web_fs_releaseDInode (inode, alfs_FSdev, 0);
         if (nextdir <= 0) {
            goto nameToInum_done;
         }
      }
      name_cache_addEntry (namecache, currdir, name, namelen, nextdir);
   }

nameToInum_done:
   if (nextdir == -1) {
      bad_request (webreq, WEBREQ_ERROR_NOTFOUND);
      nextdir = 0;
   } else if (nextdir == 0) {
/*
printf ("Time to wait for I/O request: buffer %x, blkno %d, webreq %x\n", (u_int)web_fs_waitee, web_fs_waitee->header.block, (u_int)webreq);
*/
      webreq->state |= WEBREQ_STATE_DISKWAIT;
      webreq->waitee = web_fs_waitee;
   }
   return (nextdir);
}

#endif  /* ! HIGHLEVEL */


void webreq_finddoc (webreq_t *webreq)
{
#ifndef HIGHLEVEL
   char *endofname;
   char *lastcomp;
   char *currpoint;
   int currdir;
   dinode_t *inode;
   int complen;
   int lastcomplen = 0;

web_finddoc_startover:
   endofname = &webreq->pathname[webreq->pathnamelen];
   lastcomp = (char *) webreq->lastindex;
   currpoint = lastcomp;
   currdir = webreq->tmpval;

   while (currpoint < endofname) {
/*
printf ("currpoint %p, lastcomp %p, pathname %p, endofname %p\n", currpoint, lastcomp, webreq->pathname, endofname);
printf ("pathname: %s\n", webreq->pathname);
*/
      currpoint = web_fs_path_getNextPathComponent (currpoint, &complen);

      if (complen == 0) {
         break;
      }

      if ((complen == 2) && (currpoint[0] == '.') && (currpoint[1] == '.')) {
         /* reset the pathname to kill the '..' and start over */
         bcopy (&currpoint[complen], lastcomp, ((u_int)endofname - (u_int)&currpoint[complen]));
         webreq->pathnamelen -= (currpoint - lastcomp) + complen;
         webreq->pathname[webreq->pathnamelen] = (char) 0;
         webreq->lastindex = (int) webreq->pathname;
         webreq->tmpval = DocumentRootInodeNum;
printf ("reset: pathname %s, len %d\n", webreq->pathname, webreq->pathnamelen);
         goto web_finddoc_startover;
      }
      lastcomp = currpoint;
      lastcomplen = complen;
      webreq->lastindex = (int) lastcomp;
      webreq->tmpval = (int) currdir;
      currdir = web_server_nameToInum (webreq, currdir, currpoint, complen);
      if (currdir == 0) {
         return;
      }
      currpoint += complen;
   }
   webreq->lastindex = (int) endofname;
   webreq->tmpval = currdir;

   inode = web_fs_getDInode (currdir, alfs_FSdev, (BUFFER_READ|BUFFER_ASYNC));
   if (inode == NULL) {
      webreq->state |= WEBREQ_STATE_DISKWAIT;
      webreq->waitee = web_fs_waitee;
      return;
   }
   if (inode->type == INODE_DIRECTORY) {
      if (webreq->pathname[(webreq->pathnamelen-1)] != '/') {
         bad_request (webreq, WEBREQ_ERROR_POORDIRNAME);
         return;
      }
      web_fs_releaseDInode (inode, alfs_FSdev, 0);
      currdir = web_server_nameToInum (webreq, currdir, DirectoryIndex, DirectoryIndexLen);
      if (currdir == 0) {
         return;
      }
      inode = web_fs_getDInode (currdir, alfs_FSdev, (BUFFER_READ|BUFFER_ASYNC));
      if (inode == NULL) {
         webreq->state |= WEBREQ_STATE_DISKWAIT;
         webreq->waitee = web_fs_waitee;
         return;
      }
   }
/*
printf ("resulting inodenum: %d\n", inode->dinodeNum);
*/

   webreq->inode = inode;
   webreq->lastindex = 512;  /* bypass HTTP header contained at front of file */
   webreq->state = WEBREQ_STATE_SENDDOC;
   webreq->waitee = NULL;
/*
printf ("moving to senddoc: length %d\n", inode->length);
*/
#ifdef OLD_HEADER_CONSTRUCT
   web_construct_header (webreq);
#else
   webreq->tmpval = 0;
#endif

#else	/* HIGHLEVEL */
   while (*((char *)webreq->lastindex) == '/') {
      webreq->lastindex++;
   }
   webreq->inode = open((char *)webreq->lastindex, O_RDONLY, 0);
   if (webreq->inode < 0) {
      printf ("webreq_finddoc: Could not open file (%s)\n", (char *)webreq->lastindex);
      bad_request (webreq, WEBREQ_ERROR_NOTFOUND);
      return;
   }
/*
printf ("finddoc succeeded: opened %s\n", (char *)webreq->lastindex);
*/
   web_construct_header (webreq);
   webreq->state = WEBREQ_STATE_SENDDOC;
#endif
}


void oldstyle_request (webreq_t *webreq, int i)
{
   if (webreq->command != WEBREQ_TYPE_GET) {
      printf ("Unacceptable command for oldstyle request: %d\n", webreq->command);
      webreq->command = WEBREQ_TYPE_OLDGET;
      bad_request (webreq, WEBREQ_ERROR_UNKNOWNREQ);
      return;
   }
   webreq->lastindex = (int) webreq->pathname;
#ifndef HIGHLEVEL
   webreq->tmpval = DocumentRootInodeNum;
#endif
   webreq->pathnamelen = i - (webreq->pathname - webreq->inbuf);
   webreq->inbuf[i] = (char) 0;
   webreq->command = WEBREQ_TYPE_OLDGET;
   webreq->state = WEBREQ_STATE_FINDDOC;
}


#define skipspaces(buf, i) \
	while (buf[i] == ' ') { \
	   i++; \
	}

#define web_findspace(buf, i, maxlen, lastindex, oreol) \
	while (buf[i] != ' ') { \
	   if ((buf[i] == LF) || (buf[i] == CR) || (buf[i] == (char) 0)) { \
	      if ((oreol) && ((buf[i] == LF) || (buf[i] == CR))) { \
	         break; \
	      } \
	      if ((i == buflen) && ((i - lastindex) < maxlen)) { \
	         return; \
	      } \
	      printf ("problem getting input command: buf[i] %d, i %d\n", buf[i], i); \
	      webreq->command = WEBREQ_TYPE_OLDGET; \
	      bad_request (webreq, WEBREQ_ERROR_BADREQ); \
	      return; \
	   } \
	   i++; \
	}

#define web_zerocrlf(buf, i) \
	if (buf[i] == CR) { \
	   if ((i+1) == buflen) { \
	      return; \
	   } else if (buf[(i+1)] == LF) { \
	      buf[i] = (char) 0; \
	      i++; \
	   } \
	} \
	buf[i] = (char) 0; \
        i++;


#define web_findeol(buf, i, maxlen, lastindex) \
	while ((buf[i] != CR) && (buf[i] != LF)) { \
	   if (buf[i] == (char) 0) { \
	      if ((i == buflen) && ((i - lastindex) < maxlen)) { \
	         return; \
	      } \
	      printf ("problem getting input protocol\n"); \
	      bad_request (webreq, WEBREQ_ERROR_BADREQ); \
	      return; \
	   } \
	   i++; \
	}


void webreq_reqrecv (webreq_t *webreq)
{
   int i;
   char *buf = webreq->inbuf;
   int buflen = webreq->inbuflen;

   buf[buflen] = (char) 0;

DPRINTF (2, ("reqrecv: \n%s", buf));

   i = webreq->lastindex;
   if (webreq->state == WEBREQ_STATE_REQRECV_COMMAND) {
if (strcmp (buf, "close") == 0) {
   printf ("I've been told to close by some other person\n");
   exit (0);
}
      skipspaces (buf, i);
      webreq->lastindex = i;
      web_findspace (buf, i, WEB_MAX_COMMANDLEN, webreq->lastindex, 0);
      buf[i] = (char) 0;
      i++;
/*
printf ("command: %s\n", buf);
*/
      webreq->command = translate_command (&buf[webreq->lastindex]);
      if (webreq->command == WEBREQ_TYPE_UNKNOWN) {
         printf ("Unknown command: %s\n", buf);
         bad_request (webreq, WEBREQ_ERROR_UNKNOWNREQ);
         return;
      }
      webreq->state = WEBREQ_STATE_REQRECV_PATHNAME;
      webreq->lastindex = i;
   }

   if (webreq->state == WEBREQ_STATE_REQRECV_PATHNAME) {
      skipspaces (buf, i);
      webreq->pathname = &buf[i];
      web_findspace (buf, i, PATH_MAX, webreq->lastindex, 1);
      webreq->pathnamelen = i - (webreq->pathname - buf);
      webreq->lastindex = i + 1;
      if (buf[i] == ' ') {
         skipspaces (buf, i);
      }
      if ((buf[i] == LF) || (buf[i] == CR)) {
         oldstyle_request (webreq, (webreq->lastindex - 1));
printf ("oldstyle request: %s\n", webreq->pathname);
         return;
      }
      webreq->state = WEBREQ_STATE_REQRECV_PROTOCOL;
      i = webreq->lastindex;
      buf[i-1] = (char) 0;
/*
printf ("pathname: %s\n", webreq->pathname);
*/
   }

   if (webreq->state == WEBREQ_STATE_REQRECV_PROTOCOL) {
      skipspaces (buf, i);
      web_findeol (buf, i, LINE_MAX, webreq->lastindex);
      web_zerocrlf (buf, i);
/*
printf ("protocol: %s\n", &buf[webreq->lastindex]);
*/
      webreq->lastindex = i;
      webreq->tmpval = 0;	/* used to count mime headers */
      webreq->state = WEBREQ_STATE_REQRECV_MIMEHEADER;
   }

   while (webreq->state == WEBREQ_STATE_REQRECV_MIMEHEADER) {
      skipspaces (buf, i);
      if ((buf[i] == LF) || (buf[i] == CR)) {
         i++;
         if (buf[(i-1)] == CR) {
            if ((i == buflen) && (webreq->command == WEBREQ_TYPE_POST)) {
               return;
            } else if ((i != buflen) && (buf[i] == LF)) {
               i++;
            }
         }
         webreq->lastindex = (int) webreq->pathname;
#ifndef HIGHLEVEL
         webreq->tmpval = DocumentRootInodeNum;
#endif
         webreq->state = WEBREQ_STATE_FINDDOC;
         return;
      }
      if (webreq->tmpval == WEB_MAX_INPUTMIMEHEADERS) {
         printf ("too many input mime headers\n");
         bad_request (webreq, WEBREQ_ERROR_TOOMANYMIMES);
         return;
      }

      web_findeol (buf, i, LINE_MAX, webreq->lastindex);
      web_zerocrlf (buf, i);
      webreq->tmpval++;
/*
printf ("MIME header: %s\n", &buf[(webreq->lastindex)]);
*/
      translate_mimeheader (webreq, &buf[(webreq->lastindex)]);
      webreq->lastindex = i;
   }
   assert ("at end of web_reqrecv?" == 0);
}


void webreq_senddoc (webreq_t *webreq)
{
#ifndef HIGHLEVEL
   dinode_t *inode = webreq->inode;
   int resplen = (inode) ? (inode->length + inode->headerlen) : 0;
   int sent;
   int sendfile = ((webreq->command == WEBREQ_TYPE_GET) || (webreq->command == WEBREQ_TYPE_OLDGET));
   int checksum = -1;
   int flags = TCP_SEND_MAXSIZEONLY;
   int offset;
   buffer_t *buffer = NULL;

   DPRINTF (4, ("webreq_senddoc: command %d, length %d, sendfile %d, tmpval %d\n", webreq->command, ((inode) ? inode->length : -1), sendfile, webreq->tmpval));

	/* GROK - I don't believe that "OLD_HEADER_CONSTRUCT will work!! */

	/* This path is only used for HEAD requests and bad requests */
   if ((!sendfile) && (webreq->tmpval > webreq->tcb.send_offset)) {
/*
printf ("send #1\n");
*/
      sent = xio_tcp_prepDataPacket (&webreq->tcb, 
(char *) (((uint)(webreq->pathname + webreq->pathnamelen + 1 + 3) & 0xFFFFFFFC) + webreq->tcb.send_offset), (webreq->tmpval - webreq->tcb.send_offset), NULL, 0, (TCP_SEND_ALSOCLOSE|TCP_SEND_MAXSIZEONLY), -1);
      xio_tcpcommon_sendPacket (&webreq->tcb, 0);
      if (webreq->tmpval > webreq->tcb.send_offset) {
         return;
      }
   }
   if (sendfile) {
	/* Retrofit "to send" info with where the TCB says we are */
      offset = webreq->tcb.send_offset;

	/* and then continue as before... */
//#ifndef OLD_HEADER_CONSTRUCT
      while (offset < resplen) {
         int fileoff = (offset < inode->headerlen) ? offset : (offset + 512 - inode->headerlen);
         int blklen = min(((BLOCK_SIZE-16) - (max(512,fileoff)%(BLOCK_SIZE-16))), (resplen - offset - max(0,(inode->headerlen-offset))));
         buffer_t *buffer2 = NULL;
         int len1, len2;
         char *data1;
         char *data2 = NULL;
         int blklen2 = 0;
         int voloff = 0;
         if (buffer == NULL) {
            buffer = web_fs_buffer_getBlock (alfs_FSdev, inode->dinodeNum, (fileoff / (BLOCK_SIZE-16)), (BUFFER_READ|BUFFER_ASYNC));
            if (buffer == NULL) {
               DPRINTF (4, ("blocked: webreq %p, web_fs_waitee %p\n", webreq, web_fs_waitee));
               webreq->state |= WEBREQ_STATE_DISKWAIT;
               webreq->waitee = web_fs_waitee;
               return;
            }
         }

         if ((fileoff > inode->headerlen) && ((offset + blklen) < resplen) && (blklen < 1460)) {
            buffer2 = web_fs_buffer_getBlock (alfs_FSdev, inode->dinodeNum, ((fileoff / (BLOCK_SIZE-16))+1), (BUFFER_READ|BUFFER_ASYNC));
            if (buffer2 == NULL) {
               DPRINTF (4, ("blocked (2): webreq %p, web_fs_waitee %p\n", webreq, web_fs_waitee));
               webreq->state |= WEBREQ_STATE_DISKWAIT;
               webreq->waitee = web_fs_waitee;
               web_fs_buffer_releaseBlock (buffer, 0);
               return;
            }
            data2 = buffer2->buffer;
            blklen2 = min((BLOCK_SIZE-16), (resplen - (offset + blklen)));
         }

/* GROK -- refigure out how to do this later... */
//#ifndef OLD_HEADER_CONSTRUCT
         if ((offset < inode->headerlen) && (webreq->command == WEBREQ_TYPE_GET)) {
            /* Slip in the current datetime string */
            if (offset == 0) {
               bcopy (datetime, &buffer->buffer[datetimeoff], datetimelen);
            }
            voloff = datetimeoff + datetimelen;

#ifdef PRECOMPUTE_CHECKSUMS
	/* GROK -- can't use if retrans or not mult of 1460 in offset or size */
            if ((min (webreq->tcb.mss, webreq->tcb.snd_wnd) != 1460) || (offset != 0)) {
if (webreq->recompute_checksums == 0) {
kprintf ("connection can't use precomputed checksums: mss %d, snd_wnd %d, offset %d\n", webreq->tcb.mss, webreq->tcb.snd_wnd, offset);
}
               webreq->recompute_checksums = 1;
            }
            if ((webreq->recompute_checksums) || (webreq->tcb.snd_max > webreq->tcb.snd_next)) {
               checksum = -1;
            } else {
               checksum = datetimesum + *((int *)&buffer->buffer[(BLOCK_SIZE-16)]);
            }
#endif
            data1 = &buffer->buffer[offset];
            len1 = inode->headerlen - offset;
            data2 = &buffer->buffer[512];
            len2 = blklen;
         } else {
#ifdef PRECOMPUTE_CHECKSUMS
            if ((webreq->tcb.mss != 1460) || (webreq->command == WEBREQ_TYPE_OLDGET)) {
if (webreq->recompute_checksums == 0) {
kprintf ("connection can't use precomputed checksums: mss %d, snd_wnd %d, command %d\n", webreq->tcb.mss, webreq->tcb.snd_wnd, webreq->command);
}
               webreq->recompute_checksums = 1;
            }
            if ((webreq->recompute_checksums) || (webreq->tcb.snd_max > webreq->tcb.snd_next)) {
               checksum = -1;
            } else {
               int firstsum = (fileoff < (BLOCK_SIZE-16)) ? (4 - (offset / 1460)) : (4 - (((fileoff % (BLOCK_SIZE-16)) + 1459) / 1460));
               if ((blklen >= 1460) || (buffer2 == NULL)) {
                  checksum = *((int *)&buffer->buffer[(BLOCK_SIZE-(firstsum*sizeof(int)))]);
               } else {
                  firstsum = -1;
                  checksum = *((int *)&buffer2->buffer[(BLOCK_SIZE-16)]);
               }
/*
printf ("firstsum %d, i %d, headerlen %d, sum %x\n", firstsum, i, inode->headerlen, checksum);
*/
            }
#endif
            data1 = &buffer->buffer[(fileoff%(BLOCK_SIZE-16))];
            len1 = blklen;
            len2 = blklen2;
         }

#ifdef MERGE_PACKETS
                    /* first part sets "CLOSEALSO" flag appropriately */
         flags = (resplen == (len1+len2+offset)) | TCP_SEND_MAXSIZEONLY;
#endif

//printf ("send: resplen %d, fileoff %d, offset %d, headerlen %d, len1 %d, len2 %d, flags %x, checksum %x\n", resplen, fileoff, offset, inode->headerlen, len1, len2, flags, checksum);
         assert ((len1 + len2 + offset) <= resplen);

         sent = xio_tcp_prepDataPacket (&webreq->tcb, data1, len1, data2, len2, flags, checksum);
         if (! ((sent == 0) || (sent == 1460) || (sent == (len1+len2)) || ((sent == min(webreq->tcb.mss,webreq->tcb.snd_wnd)) && (webreq->recompute_checksums)))) {
            kprintf ("sent %d, len1 %d, len2 %d, flags %x\n", sent, len1, len2, flags);
         }
         assert ((sent == 0) || (sent == 1460) || (sent == (len1+len2)) || ((sent == min(webreq->tcb.mss,webreq->tcb.snd_wnd)) && webreq->recompute_checksums));
         xio_tcpcommon_sendPacket (&webreq->tcb, voloff);

#ifndef MERGE_CACHERETRANS
	/* GROK -- this is old and broken and kept only for reference */
         {int tmplen = 18384 - xio_tcpbuffer_countOutBufferedData (&webreq->tcb);
         sent = 0;
         if (tmplen >= (inode->headerlen - j)) {
            sent = xio_tcpbuffer_write (&webreq->tcb, &buffer->buffer[j], (inode->headerlen - j), webreq->write_offset);
         }
         if ((sent == (inode->headerlen - j)) && ((tmplen-sent) >= blklen)) {
            sent += xio_tcpbuffer_write (&webreq->tcb, &buffer->buffer[i], blklen, (webreq->write_offset+sent));
         }
         webreq->write_offset += sent;
         }
#endif

         offset += sent;

         if (buffer2) {
            web_fs_buffer_releaseBlock (buffer, 0);
            buffer = buffer2;
         }

         if (sent == 0) {
/*
printf ("couldn't send it all: blklen %d, sent %d\n", blklen, sent);
*/
            web_fs_buffer_releaseBlock (buffer, 0);
            return;
         }
      }
   }

#ifndef MERGE_PACKETS
#ifndef MERGE_CACHERETRANS
   if (webreq->write_offset > webreq->tcb.send_offset) {
      webreq->docloseafterwrites = 1;
   } else
#endif
   {
      int ret = xio_tcp_initiate_close (&webreq->tcb);
      assert (ret == 1);
      xio_tcpcommon_sendPacket (&webreq->tcb, 0);
   }
#endif

   webreq->max_send_offset = webreq->tcb.send_offset;
   webreq->waitee = NULL;
   webreq->state = WEBREQ_STATE_CLOSING;

#else  /* HIGHLEVEL */
   char buf[4096];
   int ret;
   if (webreq->tmpval) {
      char *header = (char *) ((uint)(webreq->pathname + webreq->pathnamelen + 1 + 3) & 0xFFFFFFFC);                                       
      ret = write (webreq->sockfd, header, webreq->tmpval);
      if (ret != webreq->tmpval) {
         printf ("write of header 'failed': ret %d, expected %d, errno %d\n", ret, webreq->tmpval, errno);
      }
      assert (ret == webreq->tmpval);
      webreq->tmpval = 0;
   }
   if (webreq->inode >= 0) {
      while ((ret = read (webreq->inode, buf, 4096)) > 0) {
         int ret2;
         if ((ret2 = write (webreq->sockfd, buf, ret)) < ret) {
            assert ((ret2 >= 0) || ((ret2 == -1) && (errno = EWOULDBLOCK)));
            if (ret2 == -1) {
kprintf ("rolling back becuase write would block\n");
               ret2 = 0;
            }
            lseek (webreq->inode, -(ret-ret2), SEEK_CUR);
            return;
         }
      }
      if ((ret = close (webreq->inode)) != 0) {
         printf ("webreq_senddoc: file close failed (fd %d, ret %d, errno %d)\n", webreq->inode, ret, errno);
         exit (0);
      }
   }
   if ((ret = close (webreq->sockfd)) != 0) {
      printf ("webreq_senddoc: socket close failed (sockfd %d, ret %d, errno %d)\n", webreq->sockfd, ret, errno);
      exit (0);
   }
   webreq->state = WEBREQ_STATE_CLOSING;
   connfds[webreq->webreqno] = -1;
#endif
}


void web_setDocumentRoot (char *pathname)
{
#ifndef HIGHLEVEL
   int oldnum = DocumentRootInodeNum;

   printf ("web_setDocumentRoot: %s\n", pathname);
   DocumentRootInode = web_fs_translateNameToInode (pathname, alfs_FSdev, alfs_currentRootInode, BUFFER_READ);
   if (DocumentRootInode == NULL) {
      printf ("DocumentRoot (%s) does not exist\n", pathname);
      exit (0);
   }
   DocumentRootInodeNum = DocumentRootInode->dinodeNum;
   web_alias_setDocumentRoot (DocumentRootInodeNum, oldnum);
#else
   if (chdir (pathname) != 0) {
      printf ("Unable to chdir() to DocumentRoot: %s (errno %d)\n", pathname, errno);
      exit (0);
   }
#endif
}


#ifndef HIGHLEVEL
int web_server_gotdata (struct tcb *tcb, char *data, int len)
{
   webreq_t *webreq = (webreq_t *) tcb;
   assert (webreq->inpacket == NULL);
#ifdef USEINPACKETS
   assert (webreq->inbuf == NULL);
   webreq->inpacket = packethandle;
   xio_net_wrap_keepPacket (&nwinfo, packethandle);
   webreq->inbuf = data;
   webreq->inbuflen = len;
   return (1);
#else
   assert ((webreq->inbuflen+len) <= WEB_REQT_INBUFSIZE);
   webreq->inbuf = &webreq->inbufspace[0];
   bcopy (data, (webreq->inbuf+webreq->inbuflen), len);
   webreq->inbuflen += len;
   return (0);
#endif
}
#endif


static char * web_pagealloc (void *ptr, int len)
{
   char *retptr = malloc (len);
   assert (len == PAGESIZ);
   return (retptr);
}


#ifndef HIGHLEVEL
static inline int chknclr_diskwait (webreq_t *webreq)
{
   int ret = ((webreq->state & WEBREQ_STATE_DISKWAIT) && (web_fs_isdone(webreq->waitee)));
   if (ret) {
      webreq->waitee = NULL;
      webreq->state &= ~WEBREQ_STATE_DISKWAIT;
   }
   return (ret);
}
#endif


#ifndef HIGHLEVEL	/* GROK -- this is very redundant!! */

static int workon_webreq (webreq_t *webreq)
{
//printf ("working on webreq: state %d\n", webreq->state);
   assert (webreq->state != WEBREQ_STATE_FREE);
#ifndef HIGHLEVEL
   if (xio_tcp_closed (&webreq->tcb)) {
      if (webreq->tcb.retries > TCP_RETRY) {
         printf ("connection closed due to retries...\n");
      }
      xio_tcp_demux_removeconn (&dmxinfo, &webreq->tcb);
      webreq_free (webreq);
      return (1);
   }
#endif
   if (webreq->state & WEBREQ_STATE_REQRECV) {
      if (webreq->inbuflen > 0) {
         webreq_reqrecv (webreq);
      }
   }
   if (webreq->state == WEBREQ_STATE_FINDDOC) {
      webreq_finddoc (webreq);
   }
   if (webreq->state == WEBREQ_STATE_SENDDOC) {
      webreq_senddoc (webreq);
   }
   return (0);
}

#endif	/* ! HIGHLEVEL */


void web_server_setServerPort (int port)
{
#ifdef HIGHLEVEL
   assert (mainfd == -1);
   mainfd = sock_tcp_getmainport (port);
   assert (mainfd >= 0);

#else
   struct listentcb *listentcb = (struct listentcb *) malloc (sizeof(struct listentcb));
   int demux_id = xio_net_wrap_getdpf_tcp (&nwinfo, port, -1);
   assert (demux_id != -1);
   xio_tcp_initlistentcb (listentcb, INADDR_ANY, port);
   xio_tcp_demux_addlisten (&dmxinfo, (struct tcb *) listentcb, 0);
#endif
}


int main (int argc, char **argv)
{
#if 0
   pctrval ctr0;
   pctrval ctr1;
#endif
   int i, j;
   int ret;
   webreq_t *webreq;
   time_t deadtime;
   time_t tmptime;
   int deadtimeinc = 1;
#ifndef HIGHLEVEL
   struct ae_recv *packet;
#endif

#if 0
#ifndef HIGHLEVEL
   ctr0 = (P6CTR_U | P6CTR_K | P6CTR_EN | 0x6f);
   sys_wrmsr (P6MSR_CTRSEL0, &ctr0);
   ctr0 = 0;
   sys_wrmsr (P6MSR_CTR0, &ctr0);
   ctr0 = (P6CTR_U | P6CTR_K | P6CTR_EN | 0x2e | P6CTR_UM_MESI);
   sys_wrmsr (P6MSR_CTRSEL1, &ctr0);
   ctr0 = 0;
   sys_wrmsr (P6MSR_CTR1, &ctr0);
#endif
#endif

   deadtime = time(NULL);

   if (argc != 3) {
      printf ("Usage: %s <config_file> <diskno>\n", argv[0]);
      exit (0);
   }

#ifndef HIGHLEVEL
   mountFS (argv[2]);
   DocumentRootInode = alfs_currentRootInode;
   DocumentRootInodeNum = alfs_currentRootInode->dinodeNum;
#endif

#ifndef HIGHLEVEL
   xio_tcp_demux_init (&dmxinfo, 0);
   twinfo = (xio_twinfo_t *) malloc (xio_twinfo_size (4096));
   /* pass in space for hash-table of packets in time-wait */
   xio_tcp_timewait_init (twinfo, 4096, web_pagealloc);
   /* pass in pages for receive rings */
   xio_net_wrap_init (&nwinfo, malloc(64*PAGESIZ), (64 * PAGESIZ));
#endif

   web_config (argv[1]);

printf ("CONFIG DONE!\n");

   web_error_initmsgs ();

#ifndef HIGHLEVEL
   namecache = name_cache_init (16384, 128);
   name_cache_addEntry (namecache, 0, "/", 1, DocumentRootInodeNum);

#else	/* HIGHLEVEL */
   assert (mainfd >= 0);	/* must have init'd the listen by now */
   for (i=0; i<WEB_MAX_CONNS; i++) {
      connfds[i] = -1;
   }
#endif

   StaticAssert (sizeof(webreq_t) <= WEB_REQT_SIZE);

   for (i=0; i<numblocks(MaxConns,(PAGESIZ/WEB_REQT_SIZE)); i++) {
      webreq = (webreq_t *) malloc (PAGESIZ);
      assert (webreq != NULL);
      for (j=0; j<(PAGESIZ/WEB_REQT_SIZE); j++) {
         bzero ((char *)&webreq[j], sizeof(webreq_t));
         webreq[j].webreqno = (i * (PAGESIZ/WEB_REQT_SIZE)) + j;
         webreq[j].state = WEBREQ_STATE_FREE;
#ifndef HIGHLEVEL
         xio_tcp_inittcb (&webreq[j].tcb);
         webreq[j].tcb.rcv_wnd = 1460;
#endif
         webreq[j].next = free_webreqs;
         free_webreqs = &webreq[j];
      }
   }

   web_datetime_init ();

printf ("Entering main loop\n");

   while (1) {

/*
*/
tmptime = time(NULL);
if (tmptime >= deadtime) {
#ifndef HIGHLEVEL
   extern int inpackets, outpackets;
   printf ("****** Hit deadtime %d (in %d, out %d) (numconns %d, timewaiters %d, diskreqs %d)\n", (uint) deadtime, inpackets, outpackets, numconns, twinfo->timewaiter_cnt, web_fs_diskreqs_outstanding);
#else
   printf ("****** Hit deadtime %d (numconns %d)\n", (uint) deadtime, numconns);
#endif
   deadtime += deadtimeinc;
   deadtimeinc = min (1800, (deadtimeinc*2));
}

#ifdef HIGHLEVEL
      web_datetime_update();
#else
      if (((i = web_datetime_update ())) || (web_fs_diskreqs_outstanding)) {
         webreq_t *next = active_webreqs;
         while ((webreq = next)) {
            next = webreq->next;
            ret = chknclr_diskwait (webreq);
            if (xio_tcp_timedout (&webreq->tcb)) {
               ret |= xio_tcp_timeout (&webreq->tcb);
	       /* send new packet (if any) prepared by xio_tcp_timeout */
               xio_tcpcommon_sendPacket (&webreq->tcb, 0);
            }
            if (ret != 0) {
               if ((webreq->state == WEBREQ_STATE_CLOSING) && (webreq->tcb.send_offset < webreq->max_send_offset)) {
                  webreq->state = WEBREQ_STATE_SENDDOC;
               }
               workon_webreq (webreq);
            }
         }
         xio_tcpcommon_TWtimeout (twinfo, &nwinfo);
      }
#endif

#ifndef HIGHLEVEL

#ifndef BUSYWAIT
      if (web_fs_diskreqs_outstanding == 0) {
		/* GROK -- this should be the tcb with the shortest timeout */
         struct tcb *tcb = (active_webreqs) ? &active_webreqs->tcb : NULL;
		/* GROK - hack to wait for a packet arrival (or timeout).   */
		/*        need to add disk I/O.                             */
		/* GROK - busywait for a short time if packet are expected  */
		/*        soon (too avoid cswitching back and forth).       */
         assert ((tcb) || (numconns == 0));
         xio_tcp_waitforChange (&nwinfo, __ticks2usecs (__sysinfo.si_system_ticks) + TIMEOUT_POLL_INTERVAL);
      }
#endif

      /* pull a packet from the receive q */
      while ((packet = xio_net_wrap_getPacket(&nwinfo)) != NULL) {
         int keepit = 0;
//printf ("got packet: len %d\n", packet->r[0].sz);
	 /* use a hash table to find the right tcb */
         webreq = (webreq_t *) xio_tcp_demux_searchconns (&dmxinfo, packet->r[0].data);
//printf ("demux done: webreq %p\n", webreq);
	 /* make sure lengths and checksums are ok */
         if (!xio_tcp_packetcheck (packet->r[0].data, packet->r[0].sz, NULL)) {
            kprintf ("tcp packetcheck failed -- toss it\n");
         } else if (webreq) { /* webreq is null if this is a new request */
            chknclr_diskwait (webreq);
	    /* handle ack,fin,rst,seq no's,window updates, and prepare an ack */
            xio_tcp_handlePacket (&webreq->tcb, packet->r[0].data, 0);
            if (webreq->tcb.indata) {
               int livedatalen = xio_tcp_getLiveInDataLen (&webreq->tcb);
               char *livedata = xio_tcp_getLiveInData (&webreq->tcb);
               keepit = web_server_gotdata (&webreq->tcb, livedata, livedatalen);
            }
            if (webreq->tcb.send_ready) {
#ifdef MERGE_PACKETS
		/* GROK - try to further combine with packets below (closes/data sends) */
               if (!(webreq->state & WEBREQ_STATE_REQRECV))
#else
               if (1)
#endif
               {
                  xio_tcpcommon_sendPacket (&webreq->tcb, 0);
               }
               webreq->tcb.send_ready = 0;
            }

            if (webreq->tcb.state == TCB_ST_TIME_WAIT) {
               xio_tcp_timewait_add (twinfo, &webreq->tcb, -1);
               webreq->tcb.state = TCB_ST_CLOSED;
            }

#ifndef MERGE_CACHERETRANS
            if (webreq->write_offset > webreq->tcb.send_offset) {
               int ret = xio_tcpbuffer_write(&webreq->tcb, NULL, 0, webreq->write_offset);
               assert (ret == 0);
            } else if (webreq->docloseafterwrites) {
               int ret = xio_tcp_initiate_close (&webreq->tcb);
               assert (ret == 1);
               xio_tcpcommon_sendPacket (&webreq->tcb, 0);
               webreq->docloseafterwrites = 0;
            }
#endif
            workon_webreq (webreq);

         } else if (xio_tcpcommon_handleTWPacket (twinfo, packet->r[0].data, &nwinfo)) {
         } else {
            int netcardno;
            if (xio_tcpcommon_findlisten (&dmxinfo, packet->r[0].data, &nwinfo,
 &netcardno)) {
               webreq_t *newreq = webreq_alloc ();
               newreq->tcb.netcardno = netcardno;
               ret = xio_tcp_handleListenPacket (&newreq->tcb, packet->r[0].data);
               xio_tcpcommon_sendPacket (&newreq->tcb, 0);
               xio_tcp_demux_addconn (&dmxinfo, &newreq->tcb, 0);
            } else {
               xio_tcpcommon_handleRandPacket (packet->r[0].data, &nwinfo);
            }
         }
         if (keepit == 0) {
            xio_net_wrap_returnPacket (&nwinfo, packet);
         }
//printf ("packet processing done\n");
      }
#else	/* HIGHLEVEL */
      webreq = active_webreqs;
      i = 0;
      while (webreq) {
         if (webreq->state == WEBREQ_STATE_SENDDOC) {
            writefds[i] = webreq->sockfd;
            i++;
         }
         webreq = webreq->next;
      }
      for (;i<WEB_MAX_CONNS; i++) {
         writefds[i] = -1;
      }
#if 0
      sock_tcp_pollforevent (mainfd, connfds, numconns, writefds, (numconns == 0));
#else
      sock_tcp_pollforevent (mainfd, connfds, numconns, writefds, 1);
#endif
            
      if ((numconns < MaxConns) && (sock_tcp_newconn (mainfd))) {
         webreq = webreq_alloc ();
         webreq->sockfd = sock_tcp_acceptnewconn (mainfd);
/*
printf ("New connection accepted. webreq == %p\n", webreq); 
*/
         connfds[webreq->webreqno] = webreq->sockfd;
         webreq->inbuf = &webreq->inbufspace[0];
      }

      if ((i = sock_tcp_connready (connfds, numconns)) != -1) {
         webreq = active_webreqs;
         while ((webreq) && (webreq->webreqno != i)) {
            webreq = webreq->next;
         }
         assert (webreq);
         if (webreq->state & WEBREQ_STATE_REQRECV) {
            ret = sock_tcp_getdata (webreq->sockfd, &webreq->inbuf[webreq->inbuflen], (WEB_REQT_INBUFSIZE - webreq->inbuflen - 1));
            webreq->inbuflen += ret;
/*
            ret = sock_tcp_readycount (webreq->sockfd);
            assert (ret >= 0);
            if (ret > 0) {
               ret = sock_tcp_getdata (webreq->sockfd, &webreq->inbuf[webreq->inbuflen], min(ret, (WEB_REQT_INBUFSIZE - webreq->inbuflen - 1)));
               webreq->inbuflen += ret;
            }
*/
            webreq_reqrecv (webreq);
         }
         if (webreq->state == WEBREQ_STATE_FINDDOC) {
            webreq_finddoc (webreq);
         }
      }

      webreq = active_webreqs;
      while (webreq) {
         if (webreq->state == WEBREQ_STATE_SENDDOC) {
            webreq_senddoc (webreq);
         }
         if (webreq->state == WEBREQ_STATE_CLOSING) {
            webreq_t *tmp = webreq;
            webreq = webreq->next;
            webreq_free (tmp);
            continue;
         }
         webreq = webreq->next;
      }

#endif
   }
}





