/*
 *   pcap-exo.c
 *
 */
	
#include <xio/xio_net_wrap.h>
#include <xok/sysinfo.h>
#include <exos/cap.h>
#include <exos/process.h>
#include <exos/vm-layout.h>
#include <exos/net/if.h>
#include <xok/bitvector.h>

#include <sys/param.h>
#include <sys/time.h>

#include <assert.h>
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "pcap-int.h"
                        
#include "gnuc.h"
#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

//-------------------------------------------------------------------------------

int
pcap_stats(pcap_t *p, struct pcap_stat *ps)
{

        *ps = p->md.stat;  
        return (0);
}

int 
pcap_read (pcap_t *p, int cnt, pcap_handler callback, u_char *user)
{
	register u_char *bp;
	register int bufsize;
	register int cc;
	register int caplen;
	struct ae_recv *packet;
	int fromlen;
	void * tmp;

 
	bp = p->buffer + p->offset;
	bufsize = p->bufsize;
	if(p->md.pad > 0){
		memset(bp, 0, p->md.pad);
		bp += p->md.pad;
		bufsize -= p->md.pad;
	}

        do{
          xio_net_wrap_waitforPacket (p->nwinfo);
	  packet = xio_net_wrap_getPacket(p->nwinfo);
	} while(packet == NULL);

	fromlen = ae_recv_datacnt(packet);

	cc = ae_recv_datacpy(packet, bp, 0, fromlen);
	xio_net_wrap_returnPacket (p->nwinfo, packet);
	
	/* If we need have leading zero bytes, adjust count */
        cc += p->md.pad;
        bp = p->buffer + p->offset;
                
        /* If we need to step over leading junk, adjust count and pointer */
        cc -= p->md.skip;
        bp += p->md.skip;
                        
        /* Captured length can't exceed our read buffer size */
        caplen = cc;
        if (caplen > bufsize)
                caplen = bufsize;
                        
        /* Captured length can't exceed the snapshot length */
        if (caplen > p->snapshot)
                caplen = p->snapshot;
                        
        if (p->fcode.bf_insns == NULL ||
            bpf_filter(p->fcode.bf_insns, bp, cc, caplen)) {
                struct pcap_pkthdr h;
           
                ++p->md.stat.ps_recv;

	
		/* Gets timestamp  */
		 tmp = NULL;

		 gettimeofday( &h.ts , tmp);


		h.len = cc;
		h.caplen = caplen;
		(*callback)(user, &h, bp);
		return (1);
	}
	return (0);
}

int
device_lookup(char *device)
{
   int netcardno;

  //  printf("DEV LOOKUP");

   netcardno = get_ifnum_by_name (device);
   if (netcardno == -1) {
      printf ("Asking for unknown network device: %s\n", device);
      exit (-1);
   }

   return (netcardno);
}


pcap_t *
pcap_open_live(char *device, int snaplen, int promisc, int to_ms, char *ebuf)
{
	register int broadcast;
	register pcap_t *p;
        int tapno;
        uint interfaces = 0;
	int netcardno;

   //    kprintf("OPEN LIVE\n");
	
	p = (pcap_t *)malloc(sizeof(*p));
        if (p == NULL) {
                sprintf(ebuf, "malloc: %s", pcap_strerror(errno));
                return (NULL);
        }                   
        memset(p, 0, sizeof(*p));
        p->fd = -1;
	broadcast = 0;
	p->nwinfo = (void *)malloc(sizeof(xio_nwinfo_t));

     //int ExosExitHandler (OnExitHandlerFunc F, p)
	ExosExitHandler ((OnExitHandlerFunc)pcap_close, p);
     	
	xio_net_wrap_init (p->nwinfo, malloc(32 * PAGESIZ), (32 *PAGESIZ));

	
	/*
	 *  Determines the hardcoded network interface number
	 */

	netcardno = device_lookup (device);
	switch ( __sysinfo.si_networks[netcardno].cardtype ){

                case XOKNET_LOOPBACK:
	                  p->linktype = XOKNET_LOOPBACK;
			  p->md.pad = 2;
			  p->md.skip = 12;
			  p->bufsize = 1514 + 64; //Leave room for link header
			  break;

                case XOKNET_DE:
                case XOKNET_ED:
                case XOKNET_OSKIT:
                          p->linktype = XOKNET_DE;
			  p->bufsize = 4096;      //Arbitrary size given to me by Greg
			  ++broadcast;
			  break;
                default:
                          printf("Can not find network card");
                          assert(0);

	}
	
	p->buffer = (u_char *)malloc(p->bufsize + p->offset);
        if (p->buffer == NULL) {
                sprintf(ebuf, "malloc: %s", pcap_strerror(errno));
                goto bad;
        }

        bv_bs (&interfaces, netcardno);
	tapno =  xio_net_wrap_getnettap (p->nwinfo, CAP_ROOT, interfaces);
        p->xio_tapno = tapno;
	assert (tapno >= 0);

        /*  FORGET ABOUT PROMISCOUS MODE FOR NOW */

        p->md.device = strdup(device);
        if (p->md.device == NULL) {
                sprintf(ebuf, "malloc: %s", pcap_strerror(errno));
                goto bad;
        }
        p->snapshot = snaplen;

	//kprintf ("got to end of open ok\n");
         
        return (p);
	
bad:
	if (p->buffer != NULL)
                free(p->buffer);
        if (p->md.device != NULL)
                free(p->md.device);
        free(p); 
        return (NULL);

}

int
pcap_setfilter(pcap_t *p, struct bpf_program *fp)
{
  p->fcode = *fp;
  return (0);

}




















