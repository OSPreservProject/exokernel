
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


#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <exos/mallocs.h>
#include <exos/vm.h>
#include <exos/vm-layout.h>
#include <exos/uwk.h>
#include <exos/cap.h>
#include <xok/sysinfo.h>
#include <xok/sys_ucall.h>
#include <xok/mmu.h>


#include <netinet/in.h>
#include <exos/net/if.h>
#include <exos/net/ether.h>

#include "exos_tcpsocket.h"
#include "xio_tcpsocket.h"

#include <exos/kprintf.h>

#if 0
#define FDPRINT(a)	kprintf a
#else
#define FDPRINT(a)
#endif


#define TCPSOCKET_DEDICATED_REGION_END	(TCPSOCKET_DEDICATED_REGION + TCPSOCKET_DEDICATED_REGION_SZ)
#define MAININFO_PAGES		128
#define MAININFO_RINGPAGES	64
#define SPLITOUT_PAGES		128
#define SPLITOUT_RINGPAGES	64
#define SHARED_INFOS_START	(TCPSOCKET_DEDICATED_REGION + (MAININFO_PAGES * NBPG))
#define MAX_SHARED_INFOS	((TCPSOCKET_DEDICATED_REGION_SZ - (MAININFO_PAGES*NBPG)) / (SPLITOUT_PAGES*NBPG))

#define compute_infono(info)	(((uint)(info) - SHARED_INFOS_START) / \
				 (SPLITOUT_PAGES * NBPG))


typedef struct exos_tcpsock_info {
   xio_tcpsock_info_t xio_info;
   Pte myptes[MAININFO_PAGES];
   int shared_infos_cnt;
   struct exos_tcpsock_info *shared_infos[MAX_SHARED_INFOS];
   Pte shared_infos_ptes[MAX_SHARED_INFOS];
   int numpages;
   int maxpages;
   uint trusted_envid;			/* -1 means all mappers */
} exos_tcpsock_info_t;


static exos_tcpsock_info_t *privinfo = (exos_tcpsock_info_t *)TCPSOCKET_DEDICATED_REGION;


/******** Initialization functionality -- only do when actually used! *********/

static char * exos_tcpsocket_pagealloc (void *ptr, int len)
{
   exos_tcpsock_info_t *info = (exos_tcpsock_info_t *) PGROUNDDOWN((uint)ptr);
   char *newaddr = NULL;
   assert (len == NBPG);
   assert (info->xio_info.inited != 0);
   if (info->numpages < info->maxpages) {
	/* later, will have to actually alloc and map page... */
      newaddr = (char *) info + (info->numpages * NBPG);
      info->numpages++;
   }
   if (newaddr == NULL) {
      kprintf ("alloc will fail: bufcount %d, timewaiter_cnt %d, tcpsock_pages %d (%d per)\n", info->xio_info.tbinfo.bufcount, info->xio_info.twinfo.timewaiter_cnt, info->xio_info.tcpsock_pages, (NBPG/sizeof(struct tcpsocket)));
      kprintf ("numpages %d, maxpages %d, info %p\n", info->numpages, info->maxpages, info);
   }
   assert (newaddr != NULL);
   return (newaddr);
}


int exos_tcpsocket_pagefault_handler (uint va, int errcode)
{
   int pteno;
   Pte pte;
   int ret;

   kprintf ("(%d) exos_tcpsocket_pagefault_handler: va %x, errcode %x\n", __envid, va, errcode);

   if (!isvamapped(privinfo)) {
kprintf ("uh\n");
      return (0);
   }

   if (va < (TCPSOCKET_DEDICATED_REGION + (MAININFO_PAGES * NBPG))) {
      if (privinfo->xio_info.inited == 0) {
kprintf ("uh\n");
         return (0);
      }
      pteno = (va - TCPSOCKET_DEDICATED_REGION) / NBPG;
      if (pteno >= privinfo->numpages) {
kprintf ("uh\n");
         return (0);
      }
      pte = privinfo->myptes[pteno];
   } else {
      int infono = compute_infono (va);
      assert (infono < MAX_SHARED_INFOS);
      pteno = (va - SHARED_INFOS_START - (infono * SPLITOUT_PAGES * NBPG)) / NBPG;
      if (pteno == 0) {
	/* its the shared info page itself */
         pte = privinfo->shared_infos_ptes[infono];
      } else {
         exos_tcpsock_info_t *info = privinfo->shared_infos[infono];
         if (info == NULL) {
kprintf ("uh\n");
            return (0);
         }
         if (pteno >= info->numpages) {
kprintf ("uh\n");
            return (0);
         }
         pte = info->myptes[pteno];
      }
   }

   if (pte == 0) {
kprintf ("uh\n");
      return (0);
   }

   ret = _exos_self_insert_pte (CAP_ROOT, pte, PGROUNDDOWN(va), ESIP_DONTPAGE,
				NULL);
   if (ret == 0) {
      return (1);
   }
   
kprintf ("uh\n");
   return (0);
}


/* GROK -- later, avoid the "else if" case until absolutely necessary.  */
/*	(that is, when a new connection is created; all else is an bug) */
static int exos_tcpsocket_touched = 0;
#define XIO_TCPSOCKET_INIT	\
	exos_tcpsocket_touched = 1; \
	if (!isvamapped(privinfo)) { \
	   exos_tcpsocket_init (NULL, TCPSOCKET_DEDICATED_REGION, MAININFO_PAGES, MAININFO_RINGPAGES); \
	} else if (privinfo->xio_info.inited == 0) { \
	   exos_tcpsocket_init (privinfo, TCPSOCKET_DEDICATED_REGION, MAININFO_PAGES, MAININFO_RINGPAGES); \
	}


static void exos_tcpsocket_init (exos_tcpsock_info_t *info, uint vastart, int vapages, int ringpages)
{
   int ret;
   int valen = vapages * NBPG;
   int i = (info != NULL);

   StaticAssert (MAX_SHARED_INFOS > 0);
   StaticAssert (MAININFO_PAGES >= SPLITOUT_PAGES);

   vastart += ((info) ? NBPG : 0);
   valen -= ((info) ? NBPG : 0);

   assert ((info == NULL) || ((uint)info == TCPSOCKET_DEDICATED_REGION));
   assert ((uint)privinfo == TCPSOCKET_DEDICATED_REGION);

#if 0
kprintf ("(%d) exos_tcpsocket_init: valen %d, sizeof(info) %d, shared_infos[0] %p\n", __envid, valen, sizeof(exos_tcpsock_info_t), ((info) ? info->shared_infos[0] : NULL));
#endif
   assert (!isvamapped(vastart));

	/* allocate physical memory pages for the pktring */
	/*  -- these must be PG_SHARED to avoid copy-on-write after forks */
   ret = __vm_alloc_region (vastart, valen, CAP_ROOT, (PG_U|PG_P|PG_W|PG_SHARED));
   assert (ret == 0);

   if (info == NULL) {
      bzero ((char *)vastart, NBPG);
      info = (exos_tcpsock_info_t *) vastart;
   }

   for (; i<(valen/NBPG); i++) {
      info->myptes[i] = vpt[PGNO(vastart+(i*NBPG))];
   }

   xio_tcpsocket_initinfo (&info->xio_info, exos_tcpsocket_pagealloc, (info == privinfo));
   xio_net_wrap_init (&info->xio_info.nwinfo, ((char *)info + NBPG), (ringpages * NBPG));
   info->numpages = ringpages + 1;
   info->maxpages = vapages;
   info->trusted_envid = (info == privinfo) ? __envid : -1;
}


static int exos_tcpsocket_pruneshared (exos_tcpsock_info_t *maininfo)
{
   exos_tcpsock_info_t *info;
   struct tcpsocket *sock;
   int freed = 0;
   int ret;
   int i,j;

   //kprintf ("(%d) exos_tcpsocket_pruneshared: maininfo %p, refcnt %d\n", __envid, maininfo, __vm_count_refs((uint)maininfo));

   if (__vm_count_refs ((uint)maininfo) > 1) {
      return (0);
   }

   for (i=0; i<MAX_SHARED_INFOS; i++) {
      if ((info = maininfo->shared_infos[i])) {
         sock = info->xio_info.livelist;
         assert ((sock == NULL) || (sock->livenext == NULL));
         if (sock != NULL) {
            for (j=0; j<NR_OPEN; j++) {
               if ((__current->fd[j]) && (FILEP_GETSOCKPTR(__current->fd[j]) == sock)) {
                  sock = NULL;
                  break;
               }
            }
         }
         if (sock != NULL) {
            ret = __vm_free_region ((uint)info, (info->maxpages*NBPG), CAP_ROOT);
            assert (ret == 0);
            maininfo->shared_infos_cnt--;
            assert (maininfo->shared_infos_cnt >= 0);
            maininfo->shared_infos[i] = NULL;
            maininfo->shared_infos_ptes[i] = 0;
            //kprintf ("(%d) TCP pruneshared (maininfo %p, slotno %d)\n", __envid, maininfo, i);
            freed++;
         }
      }
   }
   return (freed);
}


static struct tcpsocket * exos_tcpsocket_splitout (struct tcpsocket *oldsock)
{
   exos_tcpsock_info_t *oldinfo = (exos_tcpsock_info_t *) oldsock->info;
   struct tcpsocket *newsock;
   exos_tcpsock_info_t *newinfo = NULL;
   xio_tcpbuf_t *bufpage;
   int demux_id;
   int slotno;
   int ret;
   int pteno;
   int isconn;

   exos_tcpsocket_pruneshared (oldinfo);

   for (slotno=0; slotno<MAX_SHARED_INFOS; slotno++) {
      if (oldinfo->shared_infos[slotno] == NULL) {
         newinfo = (exos_tcpsock_info_t *) (SHARED_INFOS_START + (slotno * SPLITOUT_PAGES * NBPG));
         break;
      }
   }
   assert (newinfo != NULL);
   //kprintf ("(%d) newinfo %p, slotno %d, oldinfo %p\n", __envid, newinfo, slotno, oldinfo);
   assert (((uint)newinfo >= TCPSOCKET_DEDICATED_REGION) && ((uint)newinfo < TCPSOCKET_DEDICATED_REGION_END));

   exos_tcpsocket_init (NULL, (uint)newinfo, SPLITOUT_PAGES, SPLITOUT_RINGPAGES);
   newsock = xio_tcpsocket_allocsock (&newinfo->xio_info);
   assert (newsock == &newinfo->xio_info.firstsock);

   if (oldsock->demux_id == -1) {
      assert (oldsock->tcb.state != TCB_ST_LISTEN);
      demux_id = xio_net_wrap_getdpf_tcp (&newinfo->xio_info.nwinfo, ntohs(oldsock->tcb.tcpdst), ntohs(oldsock->tcb.tcpsrc));
      assert (demux_id != -1);
	/* to deal with fact that this may be the forked off instance of the main fellow */
      if (__envid != oldinfo->trusted_envid) {
         ret = sys_dpf_ref (CAP_ROOT, demux_id, CAP_ROOT, oldinfo->trusted_envid);
         assert (ret == 0);
      }
   } else {
      demux_id = xio_net_wrap_reroutedpf (&newinfo->xio_info.nwinfo, oldsock->demux_id);
   }
   xio_tcpsocket_handlePackets (&oldinfo->xio_info);

   isconn = xio_tcp_demux_isconn (&oldsock->tcb);
   if (isconn) {
      xio_tcp_demux_removeconn (&oldinfo->xio_info.dmxinfo, &oldsock->tcb);
   }
   newsock->read_offset = oldsock->read_offset;
   newsock->write_offset = oldsock->write_offset;
   newsock->buf_max = oldsock->buf_max;
   newsock->sock_state = oldsock->sock_state;
   newsock->flags = oldsock->flags;
   newsock->demux_id = demux_id;
   if (oldsock->next != NULL) {
     kprintf ("sock->tcb.state %d sock->tcb.flags %x sock->flags %x sock->sock_state %d sock->demux_id %d sock->next->tcb.state %d sock->next->tcb.flags %x "
	     "sock->next->flags %x sock->next->sock_state %d sock->next->demux_id %d\n", oldsock->tcb.state, oldsock->tcb.flags, oldsock->flags, oldsock->sock_state,
	     oldsock->demux_id, oldsock->next->tcb.state, oldsock->next->tcb.flags, oldsock->next->flags, oldsock->next->sock_state, oldsock->next->demux_id);
   }
   assert (oldsock->next == NULL);
   xio_tcp_copytcb (&oldsock->tcb, &newsock->tcb);
   newsock->tcb.inbuffers = NULL;
   newsock->tcb.outbuffers = NULL;
   if (isconn) {
      ret = xio_tcp_demux_addconn (&newinfo->xio_info.dmxinfo, &newsock->tcb, 0);
      assert (ret == 0);
   }

   while ((bufpage = xio_tcpbuffer_yankBufPage(&oldinfo->xio_info.tbinfo, &oldsock->tcb.outbuffers))) {
	/* select a new virtual address (in self-contained space) */
      char *newaddr = exos_tcpsocket_pagealloc (newinfo, NBPG);
	/* move the physical page to new virtual address */
      ret = __vm_remap_region ((uint)bufpage, (uint)newaddr, NBPG, CAP_ROOT);
      assert (ret == 0);
	/* give it to newsock */
      xio_tcpbuffer_shoveBufPage (&newsock->info->tbinfo, &newsock->tcb.outbuffers, (xio_tcpbuf_t *) newaddr);
	/* alloc new physical page for old virtual address */
      ret = __vm_alloc_region ((uint)bufpage, NBPG, CAP_ROOT, (PG_U|PG_P|PG_W|PG_SHARED));
      assert (ret == 0);
      pteno = ((uint)bufpage - TCPSOCKET_DEDICATED_REGION) / NBPG;
      assert ((pteno > 0) && (pteno < oldinfo->numpages));
      oldinfo->myptes[pteno] = vpt[PGNO(bufpage)];
      if (__envid != oldinfo->trusted_envid) {
         int ret = __vm_share_region ((uint)bufpage, NBPG, CAP_ROOT, CAP_ROOT, oldinfo->trusted_envid, (uint)bufpage);
         assert (ret == 0);
      }
      xio_tcpbuffer_shoveFreeBufPage (&oldinfo->xio_info.tbinfo, bufpage);
   }
   while ((bufpage = xio_tcpbuffer_yankBufPage(&oldinfo->xio_info.tbinfo, &oldsock->tcb.inbuffers))) {
	/* select a new virtual address (in self-contained space) */
      char *newaddr = exos_tcpsocket_pagealloc (newinfo, NBPG);
	/* move the physical page to new virtual address */
      ret = __vm_remap_region ((uint)bufpage, (uint)newaddr, NBPG, CAP_ROOT);
      assert (ret == 0);
	/* give it to newsock */
      xio_tcpbuffer_shoveBufPage (&newsock->info->tbinfo, &newsock->tcb.inbuffers, (xio_tcpbuf_t *) newaddr);
	/* alloc new physical page for old virtual address */
      ret = __vm_alloc_region ((uint)bufpage, NBPG, CAP_ROOT, (PG_U|PG_P|PG_W|PG_SHARED));
      assert (ret == 0);
      pteno = ((uint)bufpage - TCPSOCKET_DEDICATED_REGION) / NBPG;
      assert ((pteno > 0) && (pteno < oldinfo->numpages));
      oldinfo->myptes[pteno] = vpt[PGNO(bufpage)];
      if (__envid != oldinfo->trusted_envid) {
         int ret = __vm_share_region ((uint)bufpage, NBPG, CAP_ROOT, CAP_ROOT, oldinfo->trusted_envid, (uint)bufpage);
         assert (ret == 0);
      }
      xio_tcpbuffer_shoveFreeBufPage (&oldinfo->xio_info.tbinfo, bufpage);
   }
	/* GROK -- is this needed??  is it safe?? */
   oldsock->tcb.state = TCB_ST_CLOSED;
   xio_tcpsocket_freesock (oldsock);

   oldinfo->shared_infos[slotno] = newinfo;
   oldinfo->shared_infos_ptes[slotno] = vpt[PGNO(newinfo)];
   assert (oldinfo->shared_infos_cnt >= 0);
   oldinfo->shared_infos_cnt++;
   if (__envid != oldinfo->trusted_envid) {
      int ret = __vm_share_region ((uint)newinfo, (newinfo->maxpages * NBPG), CAP_ROOT, CAP_ROOT, oldinfo->trusted_envid, (uint)newinfo);
      assert (ret == 0);
   }
   return (newsock);
}


static int exos_tcpsocket_forkhandler (u_int k, int envid, int NewPid)
{
   //kprintf ("(%d -> %d) TCP forkhandler\n", __envid, envid);
   return (0);
}


static int exos_tcpsocket_exechandler(u_int k, int envid, int execonly)
{
   int ret;
   struct file *filp;
   int shared = 0;
   int fd;
   exos_tcpsock_info_t *newprivinfo;
   uint tmpva;

   ENTERCRITICAL;

   //kprintf ("(%d -> %d) TCP exechandler\n", __envid, envid);

   if (!isvamapped(TCPSOCKET_DEDICATED_REGION)) {
      RETURNCRITICAL (0);
   }

   for (fd = 0 ; fd < NR_OPEN ; fd++) {
      filp = __current->fd[fd];
      if ((filp) && (__current->cloexec_flag[fd] == 0) && (filp->op_type == TCP_SOCKET_TYPE)) {
         shared = 1;
      }
   }

   //kprintf ("(%d -> %d) TCP exechandler: %d shared sockets\n", __envid, envid, shared);

   if (!shared) {
      RETURNCRITICAL (0);
   }

	/* set up uninited privinfo page in child */
	/* GROK -- this mallocs out of paged space, but bzero will cause */
	/* reall pages to be snagged -- is this sufficient??             */
   tmpva = (uint) __malloc (NBPG);
   assert ((tmpva % NBPG) == 0);
   newprivinfo = (exos_tcpsock_info_t *) tmpva;
   bzero ((char *)tmpva, NBPG);
   if (sys_insert_pte (CAP_ROOT, (uint)vpt[PGNO(tmpva)], TCPSOCKET_DEDICATED_REGION, k, envid) < 0) {
      assert (0);
   }

	/* move each shared socket into self-contained set of shared pages */
   shared = 0;
   for (fd = 0 ; fd < NR_OPEN ; fd++) {
      filp = __current->fd[fd];
      if ((filp) && (__current->cloexec_flag[fd] == 0) && (filp->op_type == TCP_SOCKET_TYPE)) {
         /* got a shared one.  do the thing! */
         int slot = MAX_SHARED_INFOS;
         int i;
         struct tcpsocket *sock = FILEP_GETSOCKPTR (filp);

         assert (sock);
         if (sock->info == &privinfo->xio_info) {
            /* first, must split it off */
            sock = exos_tcpsocket_splitout (sock);
         } else {
            for (i=0; i<MAX_SHARED_INFOS; i++) {
               if (&privinfo->shared_infos[i]->xio_info == sock->info) {
                  slot = i;
                  break;
               }
            }
         }
         ret = __vm_share_region ((uint)sock->info, (((exos_tcpsock_info_t *)sock->info)->maxpages * NBPG), k, k, envid, (uint)sock->info);
         if (ret < 0) {
            kprintf ("share_region failed: couldn't map into child (ret %d)\n", ret);
            assert (0);
         }

	    /* add ref for new exec'd process */
         assert (sock->demux_id != -1);
         ret = sys_dpf_ref (CAP_ROOT, sock->demux_id, k, envid);
         assert (ret == 0);

         assert (shared < MAX_SHARED_INFOS);
         newprivinfo->shared_infos[shared] = (exos_tcpsock_info_t *) sock->info;
         newprivinfo->shared_infos_ptes[shared] = vpt[PGNO(sock->info)];
         assert (newprivinfo->shared_infos_cnt >= 0);
         newprivinfo->shared_infos_cnt++;
         shared++;
         assert (((uint)sock >= TCPSOCKET_DEDICATED_REGION) && ((uint)sock < (TCPSOCKET_DEDICATED_REGION + TCPSOCKET_DEDICATED_REGION_SZ)));
         FILEP_SETSOCKPTR (filp, sock);
      }
   }

   ret = _exos_self_unmap_page(CAP_ROOT, tmpva);
   if (ret < 0) {
     kprintf("_exos_self_unmap_page failed in exos_tcpsocket_exechandler\n");
     assert(0);
   }
   __free((void*)tmpva);

   RETURNCRITICAL (0);
}


static void exos_tcpsocket_handlePackets ()
{
   int i;

   assert (isvamapped(privinfo));

   if (privinfo->xio_info.inited != 0) {
      xio_tcpsocket_handlePackets (&privinfo->xio_info);
   }

   for (i=0; i<MAX_SHARED_INFOS; i++) {
      if (privinfo->shared_infos[i]) {
         assert (privinfo->shared_infos[i]->xio_info.inited != 0);
         xio_tcpsocket_handlePackets (&privinfo->shared_infos[i]->xio_info);
      }
   }
}


/* TCP socket creator */
int exos_tcpsocket (struct file *filp)
{
   XIO_TCPSOCKET_INIT;

   FDPRINT (("tcp_socket\n"));

   StaticAssert (FILEP_TCPDATASIZE <= sizeof (filp->data));

   filp->f_mode = 0;
   filp->f_pos = 0;
   filp->f_flags = O_RDWR;
   filp_refcount_init(filp);
   filp_refcount_inc(filp);
   filp->f_owner = __current->pid;
   filp->op_type = TCP_SOCKET_TYPE;

   EnterCritical();

   FILEP_SETSOCKPTR (filp, xio_tcpsocket_allocsock (&privinfo->xio_info));

   RETURNCRITICAL (0);
}


int exos_tcpsocket_close0 (struct file *filp, int fd)
{
   struct tcpsocket *sock;

   if (filp_refcount_get(filp) == 0) {
      return (0);
   }
   assert (filp_refcount_get(filp) > 0);

   EnterCritical();

   sock = FILEP_GETSOCKPTR (filp);

   if ((sock) && (isvamapped(privinfo)) && (privinfo->shared_infos_cnt)) {
      exos_tcpsocket_pruneshared (privinfo);
   }

   //kprintf ("(%d) exos_tcpsocket_close0: f_count %d, sock %p, infono %d\n", __envid, filp_refcount_get(filp), sock, ((sock) ? compute_infono (sock->info) : -1));

   ExitCritical();
   return (0);
}


int exos_tcpsocket_close (struct file *filp)
{
   struct tcpsocket *sock;
   int ret;

   XIO_TCPSOCKET_INIT;

   EnterCritical();

   sock = FILEP_GETSOCKPTR (filp);

   assert ((sock == NULL) || (((uint)sock >= TCPSOCKET_DEDICATED_REGION) && ((uint)sock < (TCPSOCKET_DEDICATED_REGION + TCPSOCKET_DEDICATED_REGION_SZ))));
   FILEP_SETSOCKPTR (filp, NULL);

   exos_tcpsocket_handlePackets ();

   ret = xio_tcpsocket_close (sock, CHECKNB(filp));

   ExitCritical ();
   return (ret);
}


int exos_tcpsocket_bind(struct file *filp, struct sockaddr *reg_name, int namelen)
{
   struct tcpsocket *sock;
   int ret;

   XIO_TCPSOCKET_INIT;

   ENTERCRITICAL;

   exos_tcpsocket_handlePackets ();

   sock = FILEP_GETSOCKPTR (filp);
   ret = xio_tcpsocket_bind (sock, reg_name, namelen);

   RETURNCRITICAL (ret);
}


int exos_tcpsocket_select (struct file * filp, int rw)
{
   struct tcpsocket *sock;
   /* GROK - out of band sutff not implemented */
   if (rw == SELECT_EXCEPT) return 0;

   XIO_TCPSOCKET_INIT;

   ENTERCRITICAL;

   sock = FILEP_GETSOCKPTR (filp);

   exos_tcpsocket_handlePackets ();

   RETURNCRITICAL (xio_tcpsocket_select (sock, rw));
}


int exos_tcpsocket_select_pred (struct file *filp, int rw, struct wk_term *t) {
   struct tcpsocket *sock;
   int next;
   /* GROK - out of band sutff not implemented */
   if (rw == SELECT_EXCEPT) return wk_mkfalse_pred(t);

   XIO_TCPSOCKET_INIT;

   ENTERCRITICAL;

   sock = FILEP_GETSOCKPTR (filp);

   exos_tcpsocket_handlePackets ();

   next = xio_tcpsocket_select_pred (sock, rw, t);

   RETURNCRITICAL (next);
}


int exos_tcpsocket_ioctl (struct file *filp, unsigned int request, char *argp)
{
   struct tcpsocket *sock;
   int ret;

   sock = FILEP_GETSOCKPTR (filp);

   XIO_TCPSOCKET_INIT;

   ENTERCRITICAL;

   exos_tcpsocket_handlePackets ();

   ret = xio_tcpsocket_ioctl (sock, request, argp);

   RETURNCRITICAL (ret);
}


int exos_tcpsocket_recvfrom(struct file *filp, void *buffer, int length, int nonblock, unsigned flags,struct sockaddr *ignoresock, int *ignoreaddrlen)
{
   struct tcpsocket *sock;
   int ret;

   XIO_TCPSOCKET_INIT;

   ENTERCRITICAL;

   sock = FILEP_GETSOCKPTR (filp);

   exos_tcpsocket_handlePackets ();

   nonblock |= CHECKNB(filp);
   ret = xio_tcpsocket_recv (sock, buffer, length, nonblock, flags);
   RETURNCRITICAL(ret);
}


int exos_tcpsocket_read(struct file *filp, char *buffer, int length, int nonblocking)
{
   struct tcpsocket *sock;
   int ret;

   XIO_TCPSOCKET_INIT;

   ENTERCRITICAL;

   sock = FILEP_GETSOCKPTR (filp);

   exos_tcpsocket_handlePackets ();

   nonblocking |= CHECKNB(filp);
   ret = xio_tcpsocket_read (sock, buffer, length, nonblocking);
   RETURNCRITICAL(ret);
}


int exos_tcpsocket_stat (struct file *filp, struct stat *buf)
{
   struct tcpsocket *sock;
   int ret;

   XIO_TCPSOCKET_INIT;

   EnterCritical();

   sock = FILEP_GETSOCKPTR (filp);

   exos_tcpsocket_handlePackets ();

   ret = xio_tcpsocket_stat (sock, buf);

   ExitCritical ();
   return (ret);
}


int exos_tcpsocket_write(struct file *filp, char *buffer, int length, int nonblocking)
{
   struct tcpsocket *sock;
   int ret;

   XIO_TCPSOCKET_INIT;

   /* XXX - Touch all pages to make sure they can be sent out directly to the
      net without a pagefault during the critical region.  Woudln't necessarily
      have this problem if we had finer grain locking. */
   {
     int i;
     volatile char c;
     for (i=0; i < length; i += NBPG)
       c = buffer[i];
     if (length > 0) c = buffer[length-1];
   }
   ENTERCRITICAL;

   sock = FILEP_GETSOCKPTR (filp);

   exos_tcpsocket_handlePackets ();

   nonblocking |= CHECKNB(filp);
   ret = xio_tcpsocket_write (sock, buffer, length, nonblocking);
   RETURNCRITICAL (ret);
}


/* tcp_sendto ignores the destination socket parameter (and length) since */
/* tcp is a connection-oriented socket (the extra stuff is accepted just  */
/* for interface matching...)                                             */

int exos_tcpsocket_sendto(struct file *filp, void *buffer, int length, int nonblock, unsigned flags, struct sockaddr *ignoresock, int ignoreaddlen)
{
   struct tcpsocket *sock;
   int nonblocking = CHECKNB(filp);
   int ret;

   XIO_TCPSOCKET_INIT;

   ENTERCRITICAL;

   sock = FILEP_GETSOCKPTR (filp);

   exos_tcpsocket_handlePackets ();

   ret = xio_tcpsocket_send (sock, buffer, length, nonblocking, flags);
   RETURNCRITICAL (ret);
}


int exos_tcpsocket_connect(struct file *filp, struct sockaddr *uservaddr, int namelen, int flags) {
   struct tcpsocket *sock;
   struct sockaddr_in *name;
   int ret;

   ipaddr_t myip, theirip;
   ethaddr_t myeth,theireth;
   int ifnum;

   XIO_TCPSOCKET_INIT;

   ENTERCRITICAL;

   name = (struct sockaddr_in *)uservaddr;

   if (name == 0) {
      errno = EFAULT;
      RETURNCRITICAL (-1);
   }
   if (namelen != sizeof (struct sockaddr)) { 
      kprintf("exos_tcpsocket_connect: incorrect namelen it is %d should be %d\n", namelen, sizeof(struct sockaddr));
      errno = EINVAL; 
      RETURNCRITICAL (-1);
   }
   if (get_dsteth((char *)&name->sin_addr,theireth,&ifnum) != 0) {
     errno = ENETUNREACH;
     RETURNCRITICAL (-1);
   }
   memcpy(theirip, (char *)&name->sin_addr,4);
   get_ifnum_ethernet(ifnum,myeth);
   get_ifnum_ipaddr(ifnum, myip);

   if (privinfo->shared_infos_cnt > 0) {
      exos_tcpsocket_pruneshared (privinfo);
   }

   sock = FILEP_GETSOCKPTR (filp);

   exos_tcpsocket_handlePackets ();
#if 0
   kprintf("CONNECT\n");
   kprintf("myip:     %s\n",pripaddr(myip));
   kprintf("myeth:    %s\n",prethaddr(myeth));
   kprintf("theirip:  %s\n",pripaddr(theirip));
   kprintf("theireth: %s\n",prethaddr(theireth));
#endif

   ret = xio_tcpsocket_connect (sock, uservaddr, namelen, flags, CHECKNB(filp), sock->tcb.tcpdst, myip, theirip, myeth, theireth);

   RETURNCRITICAL (ret);
}


int exos_tcpsocket_fcntl (struct file * filp, int request, int arg)
{
   ENTERCRITICAL;

   FDPRINT (("exos_tcpsocket_fcntl: request %d, arg %d\n", request, arg));

   if (request == F_SETFL && arg == O_NDELAY) {
      filp->f_flags = (filp->f_flags | O_NDELAY);
   } else {
      kprintf("exos_tcpsocket_fcntl: unimplemented request (%d) and argument (%d)" "from socket.\n",request,arg);
      assert (0);
   }

   RETURNCRITICAL (0);
}


int exos_tcpsocket_getname(struct file * filp, struct sockaddr *bound, int *boundlen, int peer)
{
   struct tcpsocket *sock;
   int ret;

   ENTERCRITICAL;

   FDPRINT (("exos_tcpsocket_getname\n"));

   sock = FILEP_GETSOCKPTR (filp);

   ret = xio_tcpsocket_getname (sock, bound, boundlen, peer);
  
   RETURNCRITICAL (ret);
}


int exos_tcpsocket_setsockopt(struct file *filp, int level, int optname, const void *optval, int optlen)
{
   struct tcpsocket *sock;
   int ret;

   ENTERCRITICAL;

   sock = FILEP_GETSOCKPTR (filp);

   exos_tcpsocket_handlePackets ();

   ret = xio_tcpsocket_setsockopt (sock, level, optname, optval, optlen);
   RETURNCRITICAL (ret);
}


int exos_tcpsocket_getsockopt(struct file *filp, int level, int optname, void *optval, int *optlen)
{
   struct tcpsocket *sock;
   int ret;

   ENTERCRITICAL;

   sock = FILEP_GETSOCKPTR (filp);

   exos_tcpsocket_handlePackets ();

   ret = xio_tcpsocket_getsockopt (sock, level, optname, optval, optlen);
   RETURNCRITICAL (ret);
}


int exos_tcpsocket_shutdown(struct file *filp, int flags)
{
   struct tcpsocket *sock;
   int ret;

   ENTERCRITICAL;

   sock = FILEP_GETSOCKPTR (filp);

   exos_tcpsocket_handlePackets ();

   ret = xio_tcpsocket_shutdown (sock, flags);
   RETURNCRITICAL(ret);
}


int exos_tcpsocket_accept(struct file *filp, struct file *newfilp, struct sockaddr *newsockaddr0, int *addr_len, int flags)
{
   struct tcpsocket *sock;
   struct tcpsocket *newsock;

   XIO_TCPSOCKET_INIT;

   ENTERCRITICAL;

/*
kprintf ("(%d) exos_tcpsocket_accept\n", __envid);
*/

   sock = FILEP_GETSOCKPTR (filp);

   newfilp->f_mode = 0;
   newfilp->f_pos = 0;
   newfilp->f_flags = O_RDWR;
   filp_refcount_init(newfilp);
   filp_refcount_inc(newfilp);
   newfilp->f_owner = __current->pid;
   newfilp->op_type = TCP_SOCKET_TYPE;
   FILEP_SETSOCKPTR (newfilp, NULL);

   if (privinfo->shared_infos_cnt > 0) {
      exos_tcpsocket_pruneshared (privinfo);
   }

   exos_tcpsocket_handlePackets ();

   newsock = xio_tcpsocket_accept (sock, newsockaddr0, addr_len, flags, CHECKNB(filp));
   assert ((newsock == NULL) || (((uint)newsock >= TCPSOCKET_DEDICATED_REGION) && ((uint)newsock < (TCPSOCKET_DEDICATED_REGION + TCPSOCKET_DEDICATED_REGION_SZ))));
   FILEP_SETSOCKPTR (newfilp, newsock);
   RETURNCRITICAL ((newsock) ? 0 : -1);
}


int exos_tcpsocket_listen(struct file *filp, int backlog)
{
   struct tcpsocket *sock;
   int ret;

   //kprintf ("%d: exos_tcpsocket_listen\n", __envid);

   XIO_TCPSOCKET_INIT;

   ENTERCRITICAL;

   sock = FILEP_GETSOCKPTR (filp);

   exos_tcpsocket_handlePackets ();

   ret = xio_tcpsocket_listen (sock, backlog);

   RETURNCRITICAL (ret);
}


void exos_tcpsocket_exithandler ()
{
   exos_tcpsock_info_t *info = privinfo;
   int slotno = 0;

//kprintf ("(%d) exos_tcpsocket_exithandler: touched %d\n", __envid, exos_tcpsocket_touched);

   if (exos_tcpsocket_touched) {
      EnterCritical();
      while (info) {
         if (__vm_count_refs((uint)info) <= 1) {
            assert (__vm_count_refs((uint)info) == 1);
            xio_tcpsocket_exithandler (&info->xio_info);
         }
         do {
            info = (slotno < MAX_SHARED_INFOS) ? privinfo->shared_infos[slotno] : NULL;
            slotno++;
         } while ((info == NULL) && (slotno < MAX_SHARED_INFOS));
      }
      ExitCritical();
   }
}

void exos_tcpsocket_handleevents (u_quad_t arg)
{

  EnterCritical ();

//kprintf ("exos_tcpsocket_handleevents\n");
   if (isvamapped(privinfo)) {
      exos_tcpsocket_handlePackets ();
   }

   ExitCritical ();

}


#define WKEXTRA_MAXINFOS 4
#define WKEXTRA_MAXLEN (WKEXTRA_MAXINFOS*(XIO_TCPSOCKET_PRED_SIZE+1))

int exos_tcpsocket_wkextra (struct wk_term *t, u_quad_t arg)
{
   int sz;
   int slotno;
   exos_tcpsock_info_t *info;

//kprintf ("exos_tcpsocket_wkextra\n");

EnterCritical ();

   /* GROK -- may want to init and handle packets in this case.  What     */
   /* about the newly exec'd process that executes for a half hour before */
   /* talking to its connection??                                         */
   if ((!isvamapped(privinfo)) || (privinfo->xio_info.inited == 0)) {
      ExitCritical ();
      return (0);
   }

wkextra_repeat:

   sz = 0;
   slotno = 0;
   info = privinfo;
   exos_tcpsocket_handlePackets ();

   if (privinfo->shared_infos_cnt > (WKEXTRA_MAXINFOS-1)) {
      /* to avoid super-large predicates, cop out when many shared infos */
//kprintf ("cop out and use wk_mksleep_pred: infos %d\n", privinfo->shared_infos_cnt);
      sz = wk_mksleep_pred (t, (__sysinfo.si_system_ticks + 2));
   } else {
      while (info) {
         /* Walk thru the infos.  Handle anything that needs handling, and */
         /* build predicates as go.                                        */
         int ret = xio_tcpsocket_pred (&info->xio_info, &t[sz]);
         if (ret == -1) {
	     //kprintf ("exos_tcpsocket_wkextra: going to repeat\n");
            goto wkextra_repeat;
         }
         sz += ret;
         do {
            info = (slotno < MAX_SHARED_INFOS) ? privinfo->shared_infos[slotno] : NULL;
            slotno++;
         } while ((info == NULL) && (slotno < MAX_SHARED_INFOS));
         if (info) {
            sz = wk_mkop (sz, t, WK_OR);
         }
      }
   }

//kprintf ("%d: augmenting a wk predicate with sz %d\n", getpid(), sz);

   assert (sz <= WKEXTRA_MAXLEN);

   ExitCritical ();

   return (sz);
}


struct file_ops const exos_tcpsocket_file_ops = {
   NULL,			/* open */
   NULL,			/* lseek */
   exos_tcpsocket_read,		/* read */
   exos_tcpsocket_write,	/* write */
   exos_tcpsocket_select,	/* select */
   exos_tcpsocket_select_pred,	/* select_pred */
   exos_tcpsocket_ioctl,	/* ioctl */
   exos_tcpsocket_close0,	/* close0 */
   exos_tcpsocket_close,	/* close */
   NULL,			/* lookup */
   NULL,			/* link */
   NULL,			/* symlink */
   NULL,			/* unlink */
   NULL,			/* mkdir */
   NULL,			/* rmdir */
   NULL,			/* mknod */
   NULL,			/* rename */
   NULL,			/* readlink */
   NULL,			/* follow_link */
   NULL,			/* truncate */
   NULL,			/* dup */
   NULL,			/* release */
   NULL,			/* acquire */
   exos_tcpsocket_bind,		/* bind */
   exos_tcpsocket_connect,	/* connect */
   NULL,			/* filepair */
   exos_tcpsocket_accept,	/* accept */
   exos_tcpsocket_getname,	/* getname */
   exos_tcpsocket_listen,	/* listen */
   exos_tcpsocket_sendto,	/* sendto */
   exos_tcpsocket_recvfrom,	/* recvfrom */
   exos_tcpsocket_shutdown,	/* shutdown */
   exos_tcpsocket_setsockopt,	/* setsockopt */
   exos_tcpsocket_getsockopt,	/* getsockopt */
   exos_tcpsocket_fcntl,	/* fcntl */
   NULL,			/* mount */
   NULL,			/* unmount */
   NULL,			/* chmod */
   NULL,			/* chown */
   exos_tcpsocket_stat,		/* stat */
   NULL,			/* getdirentries */
   NULL,			/* utimes */
   NULL,			/* bmap */
   NULL,			/* fsync */
   NULL,                 	/* exithandler -- special case this to
				   very end of process exit to properly
				   handle time-waiters without making the
				   entire program wait to exit */
};


int exos_tcpsocket_fdinit(void)
{
   int ret;

   //kprintf ("exos_tcpsocket_fdinit (pid %d, eid %d)\n", getpid(), __envid);

   /* AF_UNIX, SOCK_STREAM, TCP */
   register_family_type_protocol(2,1,6,exos_tcpsocket);
   /* AF_UNIX, SOCK_STREAM, IP */
   register_family_type_protocol(2,1,0,exos_tcpsocket);
   register_file_ops((struct file_ops *)&exos_tcpsocket_file_ops, TCP_SOCKET_TYPE);
   OnExec (exos_tcpsocket_exechandler);
   OnFork (exos_tcpsocket_forkhandler);
   ret = register_pagefault_handler (TCPSOCKET_DEDICATED_REGION, TCPSOCKET_DEDICATED_REGION_SZ, exos_tcpsocket_pagefault_handler);
   assert (ret == 0);
   ret = wk_register_extra (exos_tcpsocket_wkextra, exos_tcpsocket_handleevents, 0, WKEXTRA_MAXLEN);
   assert (ret >= 0);

#if 0
{  extern int xio_net_wrap_guaranteedReceive;
   xio_net_wrap_guaranteedReceive = 0;
}
#endif
   //kprintf ("exos_tcpsocket_fdinit done\n");
   return 0;
}

